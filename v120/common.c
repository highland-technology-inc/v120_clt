#include "v120_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>

/*
 * common to some of our functions using '-m' options
 *
 * flags:
 *   DFEC_INTERM - Do not delimit with a line a dashes
 *   DFEC_NOTALL - Do not allow -m A
 */
int
do_for_each_crate(const struct v120_args_t *args,
                  int (*fn)(V120_HANDLE *, void *),
                  void *priv, unsigned int flags)
{
        int ret = EXIT_FAILURE;

        if (args->crate == 'A') {
                if (!!(flags & DFEC_NOTALL)) {
                        v120_perror("-m A not allowed for this subcommand");
                        return EXIT_FAILURE;
                }
                int count = 0;
                V120_HANDLE *v120 = NULL;
                while ((v120 = v120_next(v120)) != NULL) {
                        if (count > 0 && !(flags & DFEC_INTERM))
                                printf("---------------------\n");
                        ret = fn(v120, priv);
                        ++count;
                }
                if (count == 0)
                        goto none;
        } else {
                V120_HANDLE *v120 = args->crate < 0
                                    ? v120_next(NULL)
                                    : v120_open(args->crate);
                if (v120 == NULL)
                        goto none;
                ret = fn(v120, priv);
                v120_close(v120);
        }
        return ret;

none:
        v120_perror("no crate attached");
        return EXIT_FAILURE;
}

void
v120_perror_(const char *msg, ...)
{
        va_list args;
        va_start(args, msg);
        vfprintf(stderr, msg, args);
        va_end(args);
        if (errno)
                fprintf(stderr, " (%s)", strerror(errno));
        putc('\n', stderr);
}

int
v120_macro_wait(V120_HANDLE *v120, V120_CONFIG *cfg_regs)
{
        uint32_t val;
        do {
                val = cfg_regs->macro;
                if (val == 0x8000) {
                        v120_perror("macro command failure, reg=0x%X",
                                    val);
                        return -1;
                }
        } while(val != 0);
        return 0;
}

void
v120_macro_write(V120_HANDLE *v120, V120_CONFIG *cfg_regs, uint32_t val)
{
        cfg_regs->macro = val;
}
