#include "v120_common.h"
#include <V120.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define PTR_OFFS(P, BASE)  ((unsigned int)((void *)(P) - (void *)(BASE)))
#define VME_ACC_EMASK      (0x1FU)
#define VME_ACC_DTACK      (0x01U)
#define VME_ACKED(LA)   \
        (((LA) & VME_ACC_EMASK) == VME_ACC_DTACK)

static int
run_scan(V120_HANDLE *v120, V120_CONFIG *cfg_regs, void *vme, uint32_t *top)
{
        volatile uint32_t *ptr = (volatile uint32_t *)vme;
        int found = 0;
        int count = 0;
        uint32_t start;

        for (ptr = (volatile uint32_t *)vme; ptr < top; ptr++) {
                uint32_t lastacc;
                uint32_t x;

                x = *ptr;
                (void)x;
                lastacc = cfg_regs->vme_acc;

                if (lastacc == -1) {
                        v120_perror("Lost PCIe coms!");
                        fprintf(stderr, "Current offset: 0x%X\n",
                                PTR_OFFS(ptr, vme));
                        return -1;
                }

                if (found && !VME_ACKED(lastacc)) {
                        printf("\t0x%08X ---> 0x%08X\n",
                               start, PTR_OFFS(ptr, vme));
                        found = 0;
                } else if (!found && VME_ACKED(lastacc)) {
                        start = PTR_OFFS(ptr, vme);
                        found = 1;
                        ++count;
                }
        }

        return count;
}

static int
scan_one_helper(V120_HANDLE *v120, V120_CONFIG *cfg_regs,
                void *vme, int aw)
{
        int page;
        unsigned long top;
        V120_PD amod;
        int count;

        top = 1 << aw;
        amod = (aw == 16) ?  V120_A16 : V120_A24;

        for (page = 0; page < top / V120_PAGE_SIZE; page++) {
                v120_configure_page(v120, page, V120_PAGE_SIZE * page,
                                    V120_SFAST | V120_EAUTO | amod);
        }
        printf("Scanning A%d space...\n", aw);

        count = run_scan(v120, cfg_regs, vme, vme + top);
        if (count < 0)
                return -1;

        printf("\t%d cards found\n", count);
        return 0;
}

static int
scan_one(V120_HANDLE *v120, void *priv)
{
        const struct v120_args_t *args = (const struct v120_args_t *)priv;
        V120_CONFIG *cfg_regs;
        void *vme;

        cfg_regs = v120_get_config(v120);
        vme = v120_get_all_vme(v120);
        if (vme == NULL) {
                v120_perror("v120_get_all_vme()");
                return EXIT_FAILURE;
        }

        if (scan_one_helper(v120, cfg_regs, vme, 16) < 0)
                return EXIT_FAILURE;

        if (args->awidth == V120_A24) {
                if (scan_one_helper(v120, cfg_regs, vme, 24) < 0)
                        return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
}

int
v120_scan(int argc, char **argv, const struct v120_args_t *args)
{
        return do_for_each_crate(args, scan_one, (void *)args, 0);
}
