/**
 * @file
 * @brief Wrapper for creating a browser for a V120
 *
 * @author Paul Bailey
 * @date
 */
#include "struct.h"
#include <stdlib.h>
#include <stdio.h>

#define V120_RNM RCDATADIR "/V120.RNM"

/**
 * @brief Allocate a fake VME_REGION for a V120's control registers,
 * and add it to the global buffer list.
 *
 * @param h Handle to the V120.
 * @param crateid Crate ID, 0 to 15
 *
 * @note There is no return value. If a region cannot be added, the
 * function will take no action.
 */
void ctrlnew(V120_HANDLE *h, int crateid)
{
        VME_REGION *pregion = malloc(sizeof(*pregion));
        char name[14];
        struct rnm_t *rnm;

        if (pregion == NULL)
                return;

        pregion->base = (void *)v120_get_config(h);
        pregion->vme_addr = -1;
        pregion->len = sizeof(V120_CONFIG);
        snprintf(name, sizeof(name), "V120%d CONFIG", crateid);
        rnm = rnmparse(V120_RNM);
        /*
         * Note: if rnmparse() failed and rnm is NULL, then this
         * browser will have no register name data
         */
        browsernew(name, pregion, rnm, crateid);
}
