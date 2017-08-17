#include "v120_common.h"
#include <stdio.h>
#include <stdlib.h>

struct req_args_t {
        int bus;
        int fair;
        int slot0;
};

#define REQ_FAIR      (1U << 4)
#define REQ_BUS(n)    ((n) & 0x3U)
#define REQ_MKBUS(n)  REQ_BUS(n)

static int
requester(V120_HANDLE *v120, void *priv)
{
        struct req_args_t *args = (struct req_args_t *)priv;
        V120_CONFIG *cfg_regs = v120_get_config(v120);
        if (args->bus < 0) {
                /* query */
                unsigned int req = cfg_regs->requester;
                unsigned int slot = cfg_regs->slot;
                printf("BR%d", REQ_BUS(req));
                if (!!(req & REQ_FAIR))
                        printf(" FAIR");
                if (slot != 0)
                        printf(" ARB");
                putchar('\n');
        } else {
                /* set */
                unsigned int req = REQ_MKBUS(args->bus);
                if (args->fair)
                        req |= REQ_FAIR;
                cfg_regs->requester = req;
        }

        if (args->slot0)
                cfg_regs->slot = 0xFFFFFFFFUL;
        else
                cfg_regs->slot = 0;

        return EXIT_SUCCESS;
}

int
v120_requester(int argc, char **argv, const struct v120_args_t *args)
{
        struct req_args_t rargs;

        rargs.fair = args->fair;
        rargs.slot0 = args->slot0;
        if (optind >= argc) {
                if (args->fair) {
                        v120_perror("bus number not specified");
                        return EXIT_FAILURE;
                }
                rargs.bus = -1;
        } else {
                char *endptr;
                unsigned int bus = strtoul(argv[optind], &endptr, 0);
                if (bus > 3 || errno || endptr == argv[optind]) {
                        v120_perror("invalid bus number");
                        return EXIT_FAILURE;
                }
                rargs.bus = bus;
        }
        return do_for_each_crate(args, requester, &rargs,
                                 rargs.bus < 0 ? 0 : DFEC_INTERM);
}
