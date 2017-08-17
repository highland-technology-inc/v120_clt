#include "v120_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

struct rw_args_t {
        V120_HANDLE *v120;
        unsigned long long address;
        int nunits;
        int size;
        void (*print)(volatile void *);
        void (*put)(volatile void *, uint32_t);
        void (*printb)(volatile void *);
        uint32_t (*getb)(volatile void *);
};

static uint32_t
getb_byte(volatile void *src)
{
        return *(volatile uint8_t *)src;
}

static uint32_t
getb_word(volatile void *src)
{
        return *(volatile uint16_t *)src;
}

static uint32_t
getb_dword(volatile void *src)
{
        return *(volatile uint32_t *)src;
}

static void
printb_byte(volatile void *src)
{
        uint8_t v = *(volatile uint8_t *)src;
        fwrite(&v, 1, 1, stdout);
}

static void
printb_word(volatile void *src)
{
        uint16_t v = *(volatile uint16_t *)src;
        fwrite(&v, 2, 1, stdout);
}

static void
printb_dword(volatile void *src)
{
        uint32_t v = *(volatile uint32_t *)src;
        fwrite(&v, 4, 1, stdout);
}

static void
print_byte(volatile void *src)
{
        unsigned char v = *(volatile uint8_t *)src;
        printf("0x%02hhX", v);
}

static void
print_word(volatile void *src)
{
        unsigned short v = *(volatile uint16_t *)src;
        printf("0x%04hX", v);
}

static void
print_dword(volatile void *src)
{
        unsigned int v = *(volatile uint32_t *)src;
        printf("0x%08X", v);
}

static void
put_byte(volatile void *dst, uint32_t v)
{
        *(volatile uint8_t *)dst = (uint8_t)v;
}

static void
put_word(volatile void *dst, uint32_t v)
{
        *(volatile uint16_t *)dst = (uint16_t)v;
}

static void
put_dword(volatile void *dst, uint32_t v)
{
        *(volatile uint32_t *)dst = v;
}

static VME_REGION *
rw_open_vme(VME_REGION *region,
            const struct v120_args_t *args,
            struct rw_args_t *rw)
{
        if (args->crate == 'A') {
                v120_perror("invalid crate option 'A'");
                return NULL;
        }
        rw->v120 = args->crate < 0
                   ? v120_next(NULL)
                   : v120_open(args->crate);
        if (!rw->v120) {
                v120_perror("v120_open()");
                goto err_v120_open;
        }
        region->vme_addr = rw->address;
        region->start_page = 0;
        region->len = rw->nunits;
        region->config = args->speed | args->awidth;
        region->tag = "vrw";
        if (args->dwidth == VME_D16) {
                region->len *= 2;
                region->config |= V120_D16;
        } else if (args->dwidth == VME_D32) {
                region->len *= 4;
        }
        if (v120_add_vme_region(rw->v120, region) == NULL) {
                v120_perror("v120_add_vme_region()");
                goto err_vme;
        }
        if (v120_allocate_vme(rw->v120, 8191) < 0) {
                v120_perror("v120_allocate_vme()");
                goto err_vme;
        }
        return region;

err_vme:
        v120_close(rw->v120);
err_v120_open:
        return NULL;
}

static int
parse_address(int argc, char **argv, struct rw_args_t *rw)
{
        char *endptr;

        if (optind >= argc) {
                v120_perror("expected: address");
                return -1;
        }
        rw->address = strtoull(argv[optind], &endptr, 0);
        if (endptr == argv[optind] || errno) {
                v120_perror("invalid vme address '%s'", argv[optind]);
                return -1;
        }
        ++optind;
        return 0;
}

static int
set_up_dwidth(struct rw_args_t *rw, const struct v120_args_t *args)
{
        switch (args->dwidth) {
        case VME_D8EO:
        case VME_D8O:
                rw->size = 1;
                rw->put = put_byte;
                rw->print = print_byte;
                rw->getb = getb_byte;
                rw->printb = printb_byte;
                break;
        case VME_D16:
                if (!args->split) {
                        rw->size = 2;
                        rw->put = put_word;
                        rw->print = print_word;
                        rw->getb = getb_word;
                        rw->printb = printb_word;
                        break;
                }
                /* else, fall through */
        case VME_D32:
                rw->size = 4;
                rw->put = put_dword;
                rw->print = print_dword;
                rw->getb = getb_dword;
                rw->printb = printb_dword;
                break;
        default:
                BUG();
                return -1;
        }
        return 0;
}

static int
check_align(const struct rw_args_t *rw, VME_REGION *region)
{
        if (rw->size > 1 && (region->vme_addr & (rw->size - 1)) != 0) {
                v120_perror("invalid alignment");
                return -1;
        }
        return 0;
}

static int
gettok(char *buf, size_t len)
{
        char *p;
        char *end = &buf[len];
        int c;

        while ((c = getchar()) != EOF && isspace(c))
                ;
        if (c == EOF)
                return 1;

        p = buf;
        do {
                *p++ = c;
        } while ((c = getchar()) != EOF && isalnum(c) && p < end);
        *p = '\0';

        if (c != EOF && !isspace(c))
                return -1;
        return 0;
}

static int
v120_write_stdin_ascii(const struct rw_args_t *rw, volatile void *dst)
{
        char buffer[512];
        int count = 0;
        int tokres;

        while ((tokres = gettok(buffer, sizeof(buffer))) == 0) {
                char *endptr;
                uint32_t v = strtoul(buffer, &endptr, 0);
                if (endptr == buffer || errno) {
                        v120_perror("invalid %dth value '%s'",
                                    count, buffer);
                        return -1;
                }

                rw->put(dst, v);
                dst += rw->size;
                count++;
        }

        if (tokres != 1)
                v120_perror("Unexpected token after '%s'\n", buffer);

        return count;
}

static int
v120_write_stdin_binary(const struct rw_args_t *rw, volatile void *dst)
{
        char buffer[512];
        int count = 0;
        int total = 0;
        int alen = sizeof(buffer) / rw->size;
        do {
                char *pbuf;
                int i;
                count = fread(buffer, rw->size, alen, stdin);
                if (count < 0) {
                        v120_perror("fread(stdin) fail");
                        return -1;
                }
                pbuf = buffer;
                for (i = 0; i < count; i++) {
                        uint32_t v = rw->getb(pbuf);
                        rw->put(dst, v);
                        pbuf += rw->size;
                        dst += rw->size;
                }
                total += count;
        } while (count != alen);

        return total;
}

static int
v120_write_stdin(const struct rw_args_t *rw,
                 VME_REGION *region, int binary)
{
        /* parse from standard input */
        int res = EXIT_SUCCESS;
        int count;
        if (binary)
                count = v120_write_stdin_binary(rw, region->base);
        else
                count = v120_write_stdin_ascii(rw, region->base);

        if (count <= 0) {
                res = EXIT_FAILURE;
                if (count == 0)
                        v120_perror("expected: at least one value");
        }
        return res;
}

static int
v120_write_args(const struct rw_args_t *rw, VME_REGION *region,
                char **argv, int binary)
{
        int i;
        volatile void *dst = region->base;
        for (i = 0; i < rw->nunits; i++) {
                char *endptr;
                uint32_t v = strtoul(argv[optind], &endptr, 0);
                if (endptr == argv[optind] || errno) {
                        v120_perror("failed at writing %dth value '%s'",
                                    i, argv[optind]);
                        return EXIT_FAILURE;
                }
                rw->put(dst, v);
                dst += rw->size;
                optind++;
        }
        return EXIT_SUCCESS;
}

#define VME_RETRY       (1UL << 2)
#define VME_BTO         (1UL << 3)
#define VME_BERR        (1UL << 1)
#define VME_DTACK       (1UL << 0)
#define VME_AF          (1UL << 4)
#define VME_TIMER_MASK  (0xFFFF0000UL)
#define VME_TIMER_LSB   16

/* some verbose debuggery */
static void
print_trans_data(V120_HANDLE *v120)
{
        V120_CONFIG *pcfg = v120_get_config(v120);
        uint32_t transdata = pcfg->vme_acc;

        if (transdata & VME_AF)
                fprintf(stderr, "AF ");
        if (transdata & VME_RETRY)
                fprintf(stderr, "RETRY ");
        if (transdata & VME_BTO)
                fprintf(stderr, "BTO ");
        if (transdata & VME_BERR)
                fprintf(stderr, "BERR ");
        if (transdata & VME_DTACK)
                fprintf(stderr, "DTACK ");
        transdata >>= 16;
        transdata &= 0xFFFFU;
        transdata *= 8;
        fprintf(stderr, "%u ns\n", (unsigned int)transdata);
}

int
v120_write(int argc, char **argv, const struct v120_args_t *args)
{
        VME_REGION region, *pregion;
        int ret = EXIT_FAILURE;
        struct rw_args_t rw;

        if (parse_address(argc, argv, &rw) < 0)
                return EXIT_FAILURE;
        rw.nunits = optind > argc ? 0 : argc - optind;
        pregion = rw_open_vme(&region, args, &rw);
        if (!pregion)
                return EXIT_FAILURE;

        if (set_up_dwidth(&rw, args) < 0)
                goto err_region;

        if (check_align(&rw, &region))
                goto err_region;

        if (rw.nunits == 0)
                ret = v120_write_stdin(&rw, &region, args->binary);
        else
                ret = v120_write_args(&rw, &region, argv, args->binary);

        if (ret == EXIT_SUCCESS && args->vmeprint)
                print_trans_data(rw.v120);

err_region:
        v120_close(rw.v120);
        return ret;
}

int
v120_read(int argc, char **argv, const struct v120_args_t *args)
{
        VME_REGION region, *pregion;
        int ret = EXIT_FAILURE;
        char *endptr;
        volatile void *src;
        struct rw_args_t rw;
        int i;

        if (parse_address(argc, argv, &rw) < 0)
                return EXIT_FAILURE;

        if (optind >= argc) {
                rw.nunits = 1;
        } else {
                rw.nunits = strtoul(argv[optind], &endptr, 0);
                if (endptr == argv[optind] || errno || rw.nunits == 0) {
                        v120_perror("Invalid count '%s'", argv[optind]);
                        return EXIT_FAILURE;
                }
        }
        pregion = rw_open_vme(&region, args, &rw);
        if (!pregion)
                return EXIT_FAILURE;

        if (set_up_dwidth(&rw, args) < 0)
                goto err_region;

        if (check_align(&rw, &region))
                goto err_region;

        src = region.base;
        for (i = 0; i < rw.nunits; i++) {
                if (args->binary) {
                        rw.printb(src);
                } else {
                        if (i)
                                putchar(' ');
                        rw.print(src);
                }
                src += rw.size;
        }

        if (!args->binary)
                putchar('\n');

        if (args->vmeprint)
                print_trans_data(rw.v120);

        ret = EXIT_SUCCESS;

err_region:
        v120_close(rw.v120);
        return ret;
}
