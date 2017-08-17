#include "v120_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

static long
parse_awidth(const char *s)
{
        long v;
        char *endptr;
        v = strtoul(s, &endptr, 0);
        if (endptr != s && !errno) {
                switch (v) {
                case 16:
                        return V120_A16;
                case 24:
                        return V120_A24;
                case 32:
                        return V120_A32;
                default:
                        break;
                }
        }
        v120_perror("invalid awidth '%s'", s);
        exit(EXIT_FAILURE);
        return 0;
}

static long
parse_dwidth(const char *s, int *split)
{
        int local_split = 0;
        long ret = 0;

        switch (*optarg) {
        case 'b':
                ret = VME_D8EO;
                break;
        case 's':
                local_split = 1;
                ret = VME_D16;
                break;
        case 'w':
                ret = VME_D16;
                break;
        case 'l':
                ret = VME_D32;
                break;
        default:
                v120_perror("invalid dwidth '%s'", s);
                exit(EXIT_FAILURE);
        }
        *split = local_split;
        return ret;
}

static int
parse_crate(const char *s)
{
        int v;
        char *endptr;

        if (*s == 'A')
                return 'A';

        v = strtol(s, &endptr, 0);
        if (endptr == s || errno || v > 15) {
                v120_perror("invalid crate_no '%s'", s);
                exit(EXIT_FAILURE);
        }
        return v;
}

static int
parse_slot0(const char *s)
{
        switch (toupper((int)(*s))) {
        case '0':
        case 'F':
        case 'N':
                return 0;
        case '1':
        case 'T':
        case 'Y':
                return 1;
        default:
                break;
        }
        v120_perror("invalid slot0 '%s'", s);
        exit(EXIT_FAILURE);
}

static long
parse_speed(const char *s)
{
        long v;
        char *endptr;

        v = strtoul(s, &endptr, 0);
        if (endptr != s && !errno) {
                switch (v) {
                case 0:
                        return V120_SSLOW;
                case 1:
                        return V120_SMED;
                case 2:
                        return V120_SFAST;
                case 3:
                        return V120_SMAX;
                default:
                        break;
                }
        }
        v120_perror("invalid speed '%s'", s);
        exit(EXIT_FAILURE);
        return 0;
}

static void
version(FILE *fp)
{
        fprintf(fp, "%s\n", PACKAGE_NAME " version " PACKAGE_VERSION);
}

static void
usage(FILE *fp)
{
        fprintf(fp, "%s", v120_help_string);
}

static void
parse(int argc, char **argv, struct v120_args_t *args)
{
        static struct option opts[] = {
                { "awidth",   required_argument, NULL, 'a' },
                { "binary",   no_argument,       NULL, 'b' },
                { "dwidth",   required_argument, NULL, 'd' },
                { "vmeprint", no_argument,       NULL, 'e' },
                { "fpga",     no_argument,       NULL, 'f' },
                { "fair",     no_argument,       NULL, 'F' },
                { "crate_no", required_argument, NULL, 'm' },
                { "speed",    required_argument, NULL, 's' },
                { "slot0",    required_argument, NULL, 'S' },
                { "verbose",  no_argument,       NULL, 'v' },
                { "version",  no_argument,       NULL, 'V' },
                { "help",     no_argument,       NULL, '?' },
                { NULL, 0, NULL, '\0' },
        };
        int opt;

        /* Initialize args to defaults */
        /* XXX: Maybe let defaults be subcommand-specific */
        args->crate = -1;
        args->awidth = V120_A16;
        args->dwidth = VME_D16;
        args->split = 0;
        args->binary = 0;
        args->vmeprint = 0;
        args->fpga = 0;
        args->fair = 0;
        args->speed = V120_SMED;
        args->slot0 = 0;
        args->verbose = 0;
        while ((opt = getopt_long(argc, argv, "m:Vfeba:d:s:v",
                                  opts, NULL)) != -1) {
                switch (opt) {
                case 'a':
                        args->awidth = parse_awidth(optarg);
                        break;
                case 'b':
                        args->binary = 1;
                        break;
                case 'd':
                        args->dwidth = parse_dwidth(optarg, &args->split);
                        break;
                case 'e':
                        args->vmeprint = 1;
                        break;
                case 'F':
                        args->fair = 1;
                        break;
                case 'f':
                        args->fpga = 1;
                        break;
                case 'm':
                        args->crate = parse_crate(optarg);
                        break;
                case 's':
                        args->speed = parse_speed(optarg);
                        break;
                case 'S':
                        args->slot0 = parse_slot0(optarg);
                        break;
                case 'v':
                        args->verbose = 1;
                        break;
                case 'V':
                        version(stdout);
                        exit(EXIT_SUCCESS);
                        break;
                case '?':
                        usage(stdout);
                        exit(EXIT_SUCCESS);
                        break;
                default:
                        usage(stderr);
                        exit(EXIT_FAILURE);
                        break;
                }
        }
}

struct v120_subcommand_t {
        const char *name;
        int (*fn)(int argc, char **argv, const struct v120_args_t *args);
};

static int
exec_subc(int argc, char **argv,
          const struct v120_args_t *args,
          const struct v120_subcommand_t *lut)
{
        const char *subc;
        const struct v120_subcommand_t *t;

        if (optind >= argc) {
                v120_perror("expected: subcommand");
                goto finish_err;
        }

        subc = argv[optind++];
        for (t = lut; t->name != NULL; t++) {
                if (!strcmp(subc, t->name))
                        return t->fn(argc, argv, args);
        }

        v120_perror("Subcommand '%s' not found", subc);
finish_err:
        fprintf(stderr, "See 'man 1 v120' for valid subcommands\n");
        return EXIT_FAILURE;
}


static int
help(int argc, char **argv, const struct v120_args_t *args)
{
        usage(stdout);
        return EXIT_SUCCESS;
}

static int
report(int argc, char **argv, const struct v120_args_t *args)
{
        static const struct v120_subcommand_t rept_lut[] = {
                { "flash",   v120_report_flash },
                { "ident",   v120_report_ident },
                { "id",      v120_report_ident },
                { "power",   v120_report_power },
                { "uptime",  v120_report_uptime },
                { "status",  v120_report_status },
                { "monitor", v120_report_monitor },
                { "pcie",    v120_report_pcie },
                { "pci",     v120_report_pcie },
                { NULL, NULL }
        };
        return exec_subc(argc, argv, args, rept_lut);
}

static int
persist_one(V120_HANDLE *v120, void *priv)
{
        int res = v120_persist(v120);
        if (res != 0) {
                fprintf(stderr, "Cannot persist: %s\n", strerror(errno));
                return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
}

static int
persist(int argc, char **argv, const struct v120_args_t *args)
{
        return do_for_each_crate(args, persist_one, NULL, DFEC_INTERM);
}

int main(int argc, char **argv)
{
        static const struct v120_subcommand_t subc_lut[] = {
                { "flash-upgrade",    v120_flash_upgrade },
                { "report",           report },
                { "help",             help },
                { "reset",            v120_reset },
                { "write",            v120_write },
                { "read",             v120_read },
                { "sysreset",         v120_sysreset },
                { "reinit",           persist },
                { "loopback-upgrade", v120_lflash_upgrade },
                { "requester",        v120_requester },
                { "scan",             v120_scan },
                { NULL, NULL }
        };
        struct v120_args_t args;

        parse(argc, argv, &args);
        return exec_subc(argc, argv, &args, subc_lut);
}
