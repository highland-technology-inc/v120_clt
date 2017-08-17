#include <V120.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

#define LBUFSIZE 1024
#define PR_STAT  0

static void
print_buf16(const uint16_t *buf, size_t alen)
{
        int i;
        for (i = 0; i < alen; i++) {
                if (i)
                        putchar(i & 7 ? ' ' : '\n');
                printf("0x%04X", buf[i]);
        }
        putchar('\n');
}

static void
print_buf32(const uint32_t *buf, size_t alen)
{
        int i;
        for (i = 0; i < alen; i++) {
                if (i)
                        putchar(i & 7 ? ' ' : '\n');
                printf("0x%08X", buf[i]);
        }
        putchar('\n');
}

int
run(void *buf, unsigned long bufsize, unsigned long flags,
    unsigned long long address, int crate, bool silent)
{
        struct v120_dma_desc_t desc;
        V120_HANDLE *v120;

        if (crate < 0)
                v120 = v120_next(NULL);
        else
                v120 = v120_open(crate);
        if (!v120) {
                perror("v120_open|next() fail");
                return EXIT_FAILURE;
        }

        desc.flags = flags;
        desc.ptr = (__u64)buf;
        desc.size = bufsize;
        desc.next = 0LL;
        desc.vme_address = address;

        if (v120_dma_xfr(v120, &desc) < 0) {
                perror("ioctl(XFR) fail");
                return EXIT_FAILURE;
        }
        v120_close(v120);

        if (!silent) {
                if (V120_DWIDTH(desc.flags) == V120_D16)
                        print_buf16(buf, bufsize / 2);
                else
                        print_buf32(buf, bufsize / 4);
        }
        return EXIT_SUCCESS;
}

void
usage(FILE *fp, const char *progname)
{
        fprintf(fp, "Usage: %s [options]\n", progname);
        fprintf(fp, "   -a VME_ADDRESS      VME address\n");
        fprintf(fp, "   -m CRATE            Crate number, 0 to 15\n");
        fprintf(fp, "   -s                  Silent\n");
        fprintf(fp, "   -f                  Max VME speed\n");
        fprintf(fp, "   -h                  Do not increase VME address\n");
        fprintf(fp, "   -w AWIDTH           Address width\n");
        fprintf(fp, "   -d DWIDTH           Data width\n");
        fprintf(fp, "   -l LENGTH           Number of bytes to transfer\n");
        fprintf(fp, " Default is: %s -a0 -w16 -d16 -l0x%x\n",
                progname, LBUFSIZE);
}

#define bad_eval(what, arg) \
        fprintf(stderr, "Cannot evaluate " what " '%s'\n", arg)

static int
get_awid(const char *arg)
{
        char *endptr;
        int v = strtoul(arg, &endptr, 0);
        if (errno || endptr == arg) {
                bad_eval("address width", arg);
                exit(EXIT_FAILURE);
        }
        switch (v) {
        case 16:
                return V120_PD_A16;
        case 24:
                return V120_PD_A24;
        case 32:
                return V120_PD_A32;
        default:
                fprintf(stderr, "Bad awidth '%s'\n", arg);
        }
        return 0;
}

static int
get_dwid(const char *arg)
{
        char *endptr;
        int v = strtoul(arg, &endptr, 0);
        if (errno || endptr == arg) {
                bad_eval("data width", arg);
                exit(EXIT_FAILURE);
        }
        switch (v) {
        case 16:
                return V120_PD_D16 | V120_PD_ESHORT;
        case 32:
                return V120_PD_D32 | V120_PD_ELONG;
        default:
                fprintf(stderr, "Bad dwidth '%s'\n", arg);
                exit(EXIT_FAILURE);
        }
        return 0;
}

int
main(int argc, char **argv)
{
        int opt;
        int crate;
        char *endptr;
        unsigned long flags;
        unsigned long long address;
        bool silent;
        uint16_t *buf;
        unsigned long bufsize;

        flags = V120_PD_D16 | V120_PD_A16 | V120_PD_ESHORT;
        crate = -1;
        address = 0LL;
        bufsize = LBUFSIZE;
        silent = false;

        while ((opt = getopt(argc, argv, "d:w:m:a:hfsl:")) != -1) {
                switch (opt) {
                case 'w':
                        flags &= ~V120_PD_AWID_MASK;
                        flags |= get_awid(optarg);
                        break;
                case 'd':
                        flags &= ~(V120_PD_DWID_MASK | V120_PD_ENDIAN_MASK);
                        flags |= get_dwid(optarg);
                        break;
                case 'f':
                        flags |= V120_SMAX;
                        break;
                case 'm':
                        crate = atoi(optarg);
                        break;
                case 's':
                        silent = true;
                        break;
                case 'l':
                        bufsize = strtoul(optarg, &endptr, 0);
                        if (errno || endptr == optarg) {
                                bad_eval("length", optarg);
                                return EXIT_FAILURE;
                        }
                        break;
                case 'h':
                        flags |= V120_DMA_CTL_HOLD;
                        break;
                case 'a':
                        address = strtoull(optarg, &endptr, 0);
                        if (errno || endptr == optarg) {
                                bad_eval("address", optarg);
                                return EXIT_FAILURE;
                        }
                        break;
                default:
                        usage(stderr, argv[0]);
                        return EXIT_FAILURE;
                case '?':
                        usage(stdout, argv[0]);
                        return EXIT_SUCCESS;
                }
        }

        buf = malloc(bufsize);
        if (!buf) {
                perror("malloc(len)");
                return EXIT_FAILURE;
        }
        return run(buf, bufsize, flags, address, crate, silent);
}
