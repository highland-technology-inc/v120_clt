#include "v120_common.h"
#include <stdio.h>
#include <stdlib.h>

static void
full_reset(V120_HANDLE *v120, V120_CONFIG *cfg_regs, const struct v120_args_t *args)
{
        if (args->fpga) {
                v120_macro_write(v120, cfg_regs, MACRO_FPGA_RESET_CMD);
        } else {
                v120_macro_write(v120, cfg_regs, MACRO_RESET_CMD);
                printf("\t**************************************\n");
		printf("\tWait for V120 to finish rebooting,\n"
		       "\tthen reset your computer or use\n"
		       "\t``v120 reinit'' to reestablish PCIe\n"
		       "\tconnection with V120.\n");
                printf("\t**************************************\n");
        }
}

static void
sys_reset(V120_HANDLE *v120, V120_CONFIG *cfg_regs, const struct v120_args_t *args)
{
        cfg_regs->utility = 1;
}

static int
do_reset(void (*action)(V120_HANDLE *,
                        V120_CONFIG *,
                        const struct v120_args_t *),
         const struct v120_args_t *args)
{
        V120_HANDLE *v120;
        V120_CONFIG *cfg_regs;

        if (args->crate == 'A' || args->crate < 0) {
                v120_perror("for *reset, you must specify a crate number");
                return EXIT_FAILURE;
        }
        v120 = v120_open(args->crate);
        if (!v120) {
                v120_perror("v120_open()");
                return EXIT_FAILURE;
        }
        cfg_regs = v120_get_config(v120);
        action(v120, cfg_regs, args);
        v120_close(v120);
        return EXIT_SUCCESS;
}
int
v120_reset(int argc, char **argv, const struct v120_args_t *args)
{
        return do_reset(full_reset, args);
}

int
v120_sysreset(int argc, char **argv, const struct v120_args_t *args)
{
        return do_reset(sys_reset, args);
}
