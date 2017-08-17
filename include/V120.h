/*
 * V120.h - Definitions for libV120
 * Copyright (C) 2013-2015 Highland Technology, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * This software is released under the Modified BSD License, and may be
 * redistributed according to the terms stated in license.txt, which must
 * be kept with this file.
 *
 * Paul Bailey <pbailey@highlandtechnolgy.com>
 * 18 Otis St.
 * San Francisco, CA 94103-1220
 * (415) 551-1700
 */

#ifndef V120_H_INCLUDED
#define V120_H_INCLUDED

#ifndef V120_H
#define V120_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <v120_uapi.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#define V120_PAGE_SIZE      (0x4000)
#define V120_PAGE_COUNT     (8192)

struct V120_HANDLE;
typedef struct V120_HANDLE V120_HANDLE;

extern V120_HANDLE *v120_open(int unit_id);
extern V120_HANDLE *v120_next(V120_HANDLE *hV120);
extern int v120_close(V120_HANDLE *hV120);
extern int v120_irq_open(V120_HANDLE *hV120);
extern int v120_crate(V120_HANDLE *hV120);


/* **********************************************************************
 *      Userland-accessible register map
 ***********************************************************************/

/* v120_config_regs_t defined in <v120_uapi.h> */
typedef volatile struct v120_config_regs_t V120_CONFIG;

typedef struct V120_IRQ {
    volatile const uint32_t irqstatus;
    volatile uint32_t irqen;
    volatile uint32_t iackcfg;
    volatile uint32_t pciirq;
    volatile const uint32_t RESERVED[ 4 ];
    volatile const uint32_t iack_vector[ 8 ];
} V120_IRQ;

typedef struct V120_MONITOR {
    uint32_t mon_ctl;
    const uint32_t RESERVED1;
    uint32_t mon_trans;
    uint32_t mon_resp;
    uint64_t addr;
    uint64_t data;
} V120_MONITOR;

typedef struct V120_PCIE_RECORDS {
    uint32_t status;
    uint32_t address;
    uint64_t data;
} V120_PCIE_RECORDS;

/* ****************************************************************************
 * PCIE STATUS Field Descriptions
 *****************************************************************************/

    /* V120_PCIE_MON_STATUS_VALID - 1 if this is a valid transaction. 0 if the FIFO
     * is empty; in this case all other transaction data is undefined.
     */

#define V120_PCIE_MON_STATUS_VALID_LSB               (31)
#define V120_PCIE_MON_STATUS_VALID                   (0x80000000u)

    /* V120_PCIE_MON_STATUS_READ - 1 if this transaction was a read, in which case
     * DATA is read data. 0 if this transaction was a write, in which case DATA
     * is write data.
     */

#define V120_PCIE_MON_STATUS_READ_LSB                (30)
#define V120_PCIE_MON_STATUS_READ                    (0x40000000u)

    /* V120_PCIE_MON_STATUS_BAR - 1 for a transaction in BAR1. 0 for a transaction
     * in BAR0.
     */

#define V120_PCIE_MON_STATUS_BAR_LSB                 (29)
#define V120_PCIE_MON_STATUS_BAR                     (0x20000000u)

    /* V120_PCIE_MON_STATUS_START - 1 if this starts a burst.
     */

#define V120_PCIE_MON_STATUS_START_LSB               (28)
#define V120_PCIE_MON_STATUS_START                   (0x10000000u)

    /* V120_PCIE_MON_STATUS_TIMER - Time of the transaction, in 8 ns clock ticks.
     */

#define V120_PCIE_MON_STATUS_TIMER_LSB               (16)
#define V120_PCIE_MON_STATUS_TIMER_MASK              (0x0FFF0000u)
#define V120_PCIE_MON_STATUS_TIMER(x)                ((x) << V120_PCIE_MON_STATUS_TIMER_LSB)

    /* V120_PCIE_MON_STATUS_COUNT - Burst count, number of 64-bit words in this
     * atomic transfer.
     */

#define V120_PCIE_MON_STATUS_COUNT_LSB               (8)
#define V120_PCIE_MON_STATUS_COUNT_MASK              (0x00007F00u)
#define V120_PCIE_MON_STATUS_COUNT(x)                ((x) << V120_PCIE_MON_STATUS_COUNT_LSB)

    /* V120_PCIE_MON_STATUS_BYTE_EN - Bus byte enables for this transfer.
     */

#define V120_PCIE_MON_STATUS_BYTE_EN_LSB             (0)
#define V120_PCIE_MON_STATUS_BYTE_EN_MASK            (0x000000FFu)
#define V120_PCIE_MON_STATUS_BYTE_EN(x)              ((x) << V120_PCIE_MON_STATUS_BYTE_EN_LSB)


/* ****************************************************************************
 * MON_CTL Field Descriptions
 *****************************************************************************/

    /* V120_MON_CTL_IACK - Set to 1 to capture interrupt acknowledge
     * transactions.
     */

#define V120_MON_CTL_IACK_LSB                (31)
#define V120_MON_CTL_IACK                    (0x80000000u)

    /* V120_MON_CTL_READ - Set to 1 to capture read transactions.
     */

#define V120_MON_CTL_READ_LSB                (29)
#define V120_MON_CTL_READ                    (0x20000000u)

    /* V120_MON_CTL_WRITE - Set to 1 to capture write transactions.
     */

#define V120_MON_CTL_WRITE_LSB               (28)
#define V120_MON_CTL_WRITE                   (0x10000000u)

    /* V120_MON_CTL_RETRY - Set to 1 to capture transactions with a RETRY
     * response.
     */

#define V120_MON_CTL_RETRY_LSB               (15)
#define V120_MON_CTL_RETRY                   (0x00008000u)

    /* V120_MON_CTL_BTO - Set to 1 to capture transactions with a bus
     * timeout.
     */

#define V120_MON_CTL_BTO_LSB                 (14)
#define V120_MON_CTL_BTO                     (0x00004000u)

    /* V120_MON_CTL_BERR - Set to 1 to capture transactions with a BERR
     * response.
     */

#define V120_MON_CTL_BERR_LSB                (13)
#define V120_MON_CTL_BERR                    (0x00002000u)

    /* V120_MON_CTL_DTACK - Set to 1 to capture transactions with a DTACK
     * response.
     */

#define V120_MON_CTL_DTACK_LSB               (12)
#define V120_MON_CTL_DTACK                   (0x00001000u)

    /* V120_MON_CTL_CLR - Write a 1 to clear this monitor, setting all the
     * read only values to zero. This bit will clear itself upon clear
     * completion (this is quite fast, probably imperceptably so).
     */

#define V120_MON_CTL_CLR_LSB                 (2)
#define V120_MON_CTL_CLR                     (0x00000004u)

    /* V120_MON_CTL_OS - Set to 1 to set this block to one-shot mode. In
     * one-shot mode, upon completing a data capture the ENABLE bit will be
     * cleared, preventing further acquisition on this block until it is
     * explicitly set again.
     */

#define V120_MON_CTL_OS_LSB                  (1)
#define V120_MON_CTL_OS                      (0x00000002u)

    /* V120_MON_CTL_ENABLE - Set to 1 to enable capture on this block. The
     * one-shot action of the OS bit will clear this bit after capture.
     */

#define V120_MON_CTL_ENABLE_LSB              (0)
#define V120_MON_CTL_ENABLE                  (0x00000001u)

/* ****************************************************************************
 * MON_TRANS Field Descriptions
 *****************************************************************************/

    /* V120_MON_TRANS_IACK - The IACK state on the bus for the transaction.
     * If 0 then IACK was asserted, indicating an interrupt acknowledge cycle.
     */

#define V120_MON_TRANS_IACK_LSB              (17)
#define V120_MON_TRANS_IACK                  (0x00020000u)

    /* V120_MON_TRANS_WRITE - The WRITE state on the bus for the
     * transaction. If 0 then WRITE was asserted, indicating a write
     * transaction.
     */

#define V120_MON_TRANS_WRITE_LSB             (16)
#define V120_MON_TRANS_WRITE                 (0x00010000u)

    /* V120_MON_TRANS_SRC - Defines the source of the transaction.
     * Values:
     * COM - Serial communications: USB or Ethernet.
     * PIO - PCI Express PIO
     * DMA - PCI Express DMA
     */

#define V120_MON_TRANS_SRC_LSB               (12)
#define V120_MON_TRANS_SRC_MASK              (0x00003000u)
#define V120_MON_TRANS_SRC(x)                ((x) << V120_MON_TRANS_SRC_LSB)
#define V120_MON_TRANS_SRC_COM               (V120_MON_TRANS_SRC(1))
#define V120_MON_TRANS_SRC_PIO               (V120_MON_TRANS_SRC(0))
#define V120_MON_TRANS_SRC_DMA               (V120_MON_TRANS_SRC(2))

    /* V120_MON_TRANS_LWORD - The LWORD state on the bus for the
     * transaction. If 0 then LWORD was asserted, indicating a 32-bit data
     * transaction using all lanes D[31:0]
     */

#define V120_MON_TRANS_LWORD_LSB             (10)
#define V120_MON_TRANS_LWORD                 (0x00000400u)

    /* V120_MON_TRANS_DS1 - The DS1 state on the bus for the transaction. If
     * 0 then DS1 was asserted, signaling that D[15:8] should be active.
     */

#define V120_MON_TRANS_DS1_LSB               (9)
#define V120_MON_TRANS_DS1                   (0x00000200u)

    /* V120_MON_TRANS_DS0 - The DS0 state on the bus for the transaction. If
     * 0 then DS0 was asserted, signaling that D[7:0] should be active.
     */

#define V120_MON_TRANS_DS0_LSB               (8)
#define V120_MON_TRANS_DS0                   (0x00000100u)

    /* V120_MON_TRANS_AM - Address mode of the logged transaction.
     */

#define V120_MON_TRANS_AM_LSB                (0)
#define V120_MON_TRANS_AM_MASK               (0x0000003Fu)
#define V120_MON_TRANS_AM(x)                 ((x) << V120_MON_TRANS_AM_LSB)

/* ****************************************************************************
 * MON_RESP Field Descriptions
 *****************************************************************************/

    /* V120_MON_RESP_RETRY - Set to 1 when a transaction completes with a
     * RETRY response.
     */

#define V120_MON_RESP_RETRY_LSB              (2)
#define V120_MON_RESP_RETRY                  (1U<<V120_MON_RESP_RETRY_LSB)

    /* V120_MON_RESP_BTO - Set to 1 when a transaction does not complete
     * before the bus timer expires.
     */

#define V120_MON_RESP_BTO_LSB                (3)
#define V120_MON_RESP_BTO                    (1U<<V120_MON_RESP_BTO_LSB)

    /* V120_MON_RESP_BERR - Set to 1 when a transaction completes with a
     * BERR response.
     */

#define V120_MON_RESP_BERR_LSB               (1)
#define V120_MON_RESP_BERR                   (1U<<V120_MON_RESP_BERR_LSB)

    /* V120_MON_RESP_DTACK - Set to 1 when a transaction completes with a
     * DTACK response.
     */

#define V120_MON_RESP_DTACK_LSB              (0)
#define V120_MON_RESP_DTACK                  (1U<<V120_MON_RESP_DTACK_LSB)

    /* V120_MON_RESP_TIMER - Number of 8 ns clock ticks that the transaction
     * took to complete.
     */

#define V120_MON_RESP_TIMER_LSB              (16)
#define V120_MON_RESP_TIMER_MASK             (0xFFFF0000u)
#define V120_MON_RESP_TIMER(x)               ((x) << V120_MON_RESP_TIMER_LSB)

/* ****************************************************************************
 * DIPS Field Descriptions
 *****************************************************************************/

    /* V120_CTL_DIPS_LINDIP - The 8 discrete dip switches.
     */

#define V120_CTL_DIPS_LINDIP_LSB                  (8)
#define V120_CTL_DIPS_LINDIP_MASK                 (0x0000FF00u)
#define V120_CTL_DIPS_LINDIP(x)                   ((x) << V120_CTL_DIPS_LINDIP_LSB)

    /* V120_CTL_DIPS_CABLE - The PCIe cable length rotary hex switch.
     */

#define V120_CTL_DIPS_CABLE_LSB                   (4)
#define V120_CTL_DIPS_CABLE_MASK                  (0x000000F0u)
#define V120_CTL_DIPS_CABLE(x)                    ((x) << V120_CTL_DIPS_CABLE_LSB)

    /* V120_CTL_DIPS_UNIT - The unit ID rotary hex switch. This value will be
     * displayed on the front panel indicator.
     */

#define V120_CTL_DIPS_UNIT_LSB                    (0)
#define V120_CTL_DIPS_UNIT_MASK                   (0x0000000Fu)
#define V120_CTL_DIPS_UNIT(x)                     ((x) << V120_CTL_DIPS_UNIT_LSB)

/* ****************************************************************************
 * VME_ACC Field Descriptions
 *****************************************************************************/

    /* V120_CTL_VME_ACC_RETRY - Set to 1 when a transaction completes with a
     * RETRY response.
     */

#define V120_CTL_VME_ACC_RETRY_LSB                (2)
#define V120_CTL_VME_ACC_RETRY                    (1U << V120_CTL_VME_ACC_RETRY_LSB)

    /* V120_CTL_VME_ACC_BTO - Set to 1 when a transaction does not complete
     * before the bus timer expires.
     */

#define V120_CTL_VME_ACC_BTO_LSB                  (3)
#define V120_CTL_VME_ACC_BTO                      (1U << V120_CTL_VME_ACC_BTO_LSB)

    /* V120_CTL_VME_ACC_BERR - Set to 1 when a transaction completes with a BERR
     * response.
     */

#define V120_CTL_VME_ACC_BERR_LSB                 (1)
#define V120_CTL_VME_ACC_BERR                     (1U << V120_CTL_VME_ACC_BERR_LSB)

    /* V120_CTL_VME_ACC_DTACK - Set to 1 when a transaction completes with a
     * DTACK response.
     */

#define V120_CTL_VME_ACC_DTACK_LSB                (0)
#define V120_CTL_VME_ACC_DTACK                    (1U << V120_CTL_VME_ACC_DTACK_LSB)

    /* V120_CTL_VME_ACC_AF_LSB - Set to 1 when a transaction cannot complete
     * due to a failure to win arbitration on the VME bus.
     */
#define V120_CTL_VME_ACC_AF_LSB                   (0)
#define V120_CTL_VME_ACC_AF                       (1U << V120_CTL_VME_ACC_AF_LSB)

    /* V120_CTL_VME_ACC_TIMER - Number of 8 ns clock ticks that the transaction
     * took to complete.
     */

#define V120_CTL_VME_ACC_TIMER_LSB                (16)
#define V120_CTL_VME_ACC_TIMER_MASK               (0xFFFF0000u)
#define V120_CTL_VME_ACC_TIMER(x)                 ((x) << V120_CTL_VME_ACC_TIMER_LSB)

/* ****************************************************************************
 * UTILITY Field Descriptions
 *****************************************************************************/

    /* V120_CTL_UTILITY_SYSCLK_KHZ - Reads back the frequency of the (nominally)
     * 16 MHz SYSCLK line in kHz.
     */

#define V120_CTL_UTILITY_SYSCLK_KHZ_LSB           (16)
#define V120_CTL_UTILITY_SYSCLK_KHZ_MASK          (0xFFFF0000u)
#define V120_CTL_UTILITY_SYSCLK_KHZ(x)            ((x) << V120_CTL_UTILITY_SYSCLK_KHZ_LSB)

    /* V120_CTL_UTILITY_SYSCLK - Reads back the state of the 16 MHz SYSCLK line.
     */

#define V120_CTL_UTILITY_SYSCLK_LSB               (3)
#define V120_CTL_UTILITY_SYSCLK                   (0x00000008u)

    /* V120_CTL_UTILITY_ACFAIL - Reads back the state of the (active-low) ACFAIL
     * line.
     */

#define V120_CTL_UTILITY_ACFAIL_LSB               (2)
#define V120_CTL_UTILITY_ACFAIL                   (0x00000004u)

    /* V120_CTL_UTILITY_SYSFAIL - Reads back the state of the (active-low)
     * SYSFAIL line.
     */

#define V120_CTL_UTILITY_SYSFAIL_LSB              (1)
#define V120_CTL_UTILITY_SYSFAIL                  (0x00000002u)

    /* V120_CTL_UTILITY_SYSRESET - Reads back the state of the (active-low)
     * SYSRESET line. Write a 1 to this bit to initiate a SYSRESET, which will
     * hold SYSRESET low for the VME minimum of 200 ms.
     */

#define V120_CTL_UTILITY_SYSRESET_LSB             (0)
#define V120_CTL_UTILITY_SYSRESET                 (0x00000001u)

/* **********************************************************************
 *      IRQ Block Descriptions
 ***********************************************************************/

        /* irqen register */
#define V120_IRQEN_FAKE(MSK)  (((MSK) & 0xFFU) << 8)
#define V120_IRQEN_EN(MSK)    ((MSK) & 0xFFU)

        /* iackcfg register */
#define V120_IACKCFG(VEC, SPD)    \
        (((SPD) & 0x3U) << (4 * (VEC)))

/**
 * V120_PD - V120 page descriptor configuration flags.
 */
typedef uint64_t V120_PD;

#define V120_SPEED(PD)  ((PD) & V120_PD_SPEED_MASK)
#define V120_SMAX       V120_PD_SMAX
#define V120_SFAST      V120_PD_SFAST
#define V120_SMED       V120_PD_SMED
#define V120_SSLOW      V120_PD_SSLOW

#define V120_ENDIAN(PD) ((PD) & V120_PD_ENDIAN_MASK)
#define V120_EAUTO      V120_PD_EAUTO
#define V120_EBYTE      V120_PD_EBYTE
#define V120_ESHORT     V120_PD_ESHORT
#define V120_ELONG      V120_PD_ELONG

#define V120_PD_ISRW(PD) (((PD) & V120_PD_RW_MASK) == V120_PD_RW)
#define V120_RW         V120_PD_RW
#define V120_RO         V120_PD_RO

#define V120_DWIDTH(PD) ((PD) & V120_PD_DWID_MASK)
#define V120_D32        V120_PD_D32
#define V120_D16        V120_PD_D16

/**
 * V120_AM - VME Address modifier enumerations
 */
typedef enum V120_AM {
    /* TODO: Add/check address modifiers */
    V120_A16     = V120_PD_A16,
    V120_A24     = V120_PD_A24,
    V120_A32     = V120_PD_A32
} V120_AM;

#define V120_AWIDTH(PD)       ((PD) & V120_PD_AWID_MASK)
#define V120_MKAM(am)         V120_AWIDTH(V120_PD_AM_(am))

extern V120_IRQ *v120_get_irq(V120_HANDLE *hV120);
extern V120_CONFIG *v120_get_config(V120_HANDLE * hV120);
extern volatile V120_MONITOR *v120_get_monitor(V120_HANDLE *hV120, int monitor);
extern volatile V120_PCIE_RECORDS *v120_get_records(V120_HANDLE *hV120, int record);
extern char *v120_snprint_monitor(V120_HANDLE *hV120, int monitor, char *target, size_t n);
extern void v120_fprint_monitor(FILE *f, V120_HANDLE *hV120, int monitor);
static inline void v120_print_monitor(V120_HANDLE *hV120, int monitor)
{
    v120_fprint_monitor(stdout, hV120, monitor);
}
extern V120_PD *v120_get_pd(V120_HANDLE *hV120);
extern void *v120_get_all_vme(V120_HANDLE *hV120);
extern void *v120_get_vme(V120_HANDLE *hV120, int start_page, int end_page);
extern void *v120_configure_page(V120_HANDLE *hV120, int idx, uint64_t base, V120_PD config);
extern V120_PD v120_get_page_configuration(V120_HANDLE *hV120, int idx);


/**********************************************************************
 * VME DMA management
 **********************************************************************/

extern int v120_dma_status(V120_HANDLE *v120, struct v120_dma_status_t *status);
extern int v120_dma_xfr(V120_HANDLE *v120, struct v120_dma_desc_t *hdr);


/**********************************************************************
 * Hotplug kluge
 **********************************************************************/

extern int v120_persist(V120_HANDLE *v120);


/**********************************************************************
 * VME card management
 **********************************************************************/

/**
 * struct VME_REGION - information to access a single region of VME
 *                     memory.
 * @next:       Private field.  Users must not use this.
 * @base:       Pointer to the base (lowest) address of the region.  This
 *              should be initialized to NULL by the user, and will be
 *              set by the allocate_vme function.
 * @start_page: Page descriptor index (0-8191) of the first page where
 *              this region occurs.  The region will be in all contiguous
 *              pages from start_page to end_page.  This should be
 *              initialized to 0 by the user, and will be set by the
 *              allocate_vme function.
 * @end_page:   Page descriptor index (0-8191) of the last page where
 *              this region occurs.  The region will be in all contiguous
 *              pages from start_page to end_page.  This should be
 *              initialized to 0 by the user, and will be set by the
 *              allocate_vme function.
 * @vme_addr:   VME base address set on the card
 * @len:        Total size of the region in bytes
 * @config:     Page descriptor configuration flags
 * @tag:        Name or other identifying information
 * @udata:      Arbitrary user data to be attached to this card
 *
 * Usually, this will correspond one-to-one with a single VME card,
 * but some cards may require multiple regions, such as an A16
 * configuration space and an A24 or A32 data space.
 */
typedef struct VME_REGION {
    struct VME_REGION *next;
    void              *base;
    unsigned int      start_page;
    unsigned int      end_page;
    uint64_t          vme_addr;
    size_t            len;
    V120_PD           config;
    const char        *tag;
    void              *udata;
} VME_REGION;

extern VME_REGION *v120_add_vme_region(V120_HANDLE *hV120, VME_REGION *data);
extern VME_REGION *v120_get_vme_region(V120_HANDLE *hV120, const char *name);
extern void v120_delete_vme_list(V120_HANDLE *hV120);
extern int v120_allocate_vme(V120_HANDLE *hV120, unsigned int start_page);

#ifdef __cplusplus
}
#endif

#endif /* V120_H */

#endif /* V120_H_INCLUDED */
