#include "v120_common.h"
#include <stdio.h>
#include <stdlib.h>

static void
validate_crate(int crateno)
{
        /*
         * Do not use -A or -1 ("first") arg for crate.
         * Make user explicitly state which crate to flash-upgrade.
         */
        if (crateno < 0 || crateno == 'A') {
                v120_perror("For *-upgrade, you must specify a crate number");
                exit(EXIT_FAILURE);
        }
}

static int
unlock_flash(V120_HANDLE *v120, V120_CONFIG *cfg_regs)
{
        printf("Unlocking flash\n");
        v120_macro_write(v120, cfg_regs, MACRO_UNLOCK_CMD);
        return v120_macro_wait(v120, cfg_regs);
}

static int
erase_flash(V120_HANDLE *v120, V120_CONFIG *cfg_regs, uint32_t macro_off)
{
        printf("Erasing flash. This takes a while.\n");
        v120_macro_write(v120, cfg_regs, MACRO_ERASE_CMD + macro_off);
        return v120_macro_wait(v120, cfg_regs);
}

static int
reflash(V120_HANDLE *v120, V120_CONFIG *cfg_regs,
        FILE *fp, uint32_t macro_off)
{
        uint32_t pgno = 0;
        uint32_t buf[REFLASH_ARRAYLEN];
        volatile int result;
        do {
                int i;

                printf("Writing page number %u.\n", pgno);
                result = fread(buf, sizeof(uint32_t), REFLASH_ARRAYLEN, fp);
                for (i = 0; i < REFLASH_ARRAYLEN; ++i)
                        cfg_regs->buf[i] = buf[i];

                cfg_regs->mp[0] = pgno;
                v120_macro_write(v120, cfg_regs,
                                 MACRO_REFLASH_CMD + macro_off);
                result = v120_macro_wait(v120, cfg_regs);
                if (result < 0)
                        break;
                ++pgno;
        } while (!feof(fp));

        if (!result && feof(fp)) {
                printf("End of file input file reached.\n"
                       "Run 'v120 report flash' to verify, then 'v120 reset' to finish.\n"
                       "Resetting the V120 will also require a reboot of this PC\n");
        }
        return result;
}

static int
v120_flash_upgrade_(int argc, char **argv,
                    const struct v120_args_t *args, uint32_t macro_off)
{
        V120_HANDLE *v120;
        V120_CONFIG *cfg_regs;
        FILE *fp;
        int ret = EXIT_FAILURE;

        validate_crate(args->crate);
        if (optind >= argc) {
                v120_perror("expected: file name");
                return EXIT_FAILURE;
        }
        v120 = v120_open(args->crate);
        if (!v120) {
                v120_perror("v120_open()");
                goto err_v120_open;
        }
        cfg_regs = v120_get_config(v120);
        if (!cfg_regs) {
                v120_perror("v120_get_config()");
                goto err_get_config;
        }
        fp = fopen(argv[optind], "rb");
        if (!fp) {
                v120_perror("fopen(%s)", argv[optind]);
                goto err_fopen;
        }
        if (unlock_flash(v120, cfg_regs) < 0)
                goto err_oper;
        if (erase_flash(v120, cfg_regs, macro_off) < 0)
                goto err_oper;
        if (reflash(v120, cfg_regs, fp, macro_off) < 0)
                goto err_oper;

        ret = EXIT_SUCCESS;

err_oper:
        fclose(fp);
err_fopen:
err_get_config:
        v120_close(v120);
err_v120_open:
        return ret;
}

int
v120_flash_upgrade(int argc, char **argv, const struct v120_args_t *args)
{
        return v120_flash_upgrade_(argc, argv, args, 0);
}

int
v120_lflash_upgrade(int argc, char **argv, const struct v120_args_t *args)
{
        return v120_flash_upgrade_(argc, argv, args, 0x20);
}
