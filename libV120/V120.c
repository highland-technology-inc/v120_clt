/*
 * V120.c - main functions for libV120
 * Copyright (C) 2013-2014 Highland Technology, Inc.
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

#include "config.h"
#include "V120.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

/* Until I can figure out how to mmap pcie record block */
#define NO_MMAP_RECORD  (1)

/*
 * Linked list of every memory-mapped (via mmap call) pointers
 * associated with a V120 handle. Each element in the list is
 * additionally malloc'd. See comments to list handling functions
 * below.
 */
struct vme_mmlist_t {
        void                    *ptr;
        unsigned long           size;
        struct vme_mmlist_t     *next;
};

/*
 * Structure of the VME monitor block in the V120. (Part of
 * PCIe memory space)
 */
struct V120_MONITOR_BLOCK {
        struct {
                volatile V120_MONITOR mon;
                volatile const uint32_t RESERVED2[8];
        } mon[4];
};

/*
 * Structure of the PCIe record block in the V120. (Part of
 * PCIe memory space)
 */
struct V120_PCIE_RECORD_BLOCK {
        volatile V120_PCIE_RECORDS record[128];
};

/*
 * Handle whose pointer is visible to user (see documentation
 * in V120.h). The fields are private here.
 */
struct V120_HANDLE {
        /*
         * linked list of VME cards mapped (not same as mmlist, which
         * takes care of mmap management)
         */
        struct VME_REGION               *list_head;
        /* file descriptor of v120_c(n) device (control region) */
        int                             fd_ctrl;
        /* file descriptor of v120_v(n) device (VME pages) */
        int                             fd_vme;
        /* file descriptor of v120_q(n) device (IRQ ) */
        int                             fd_irq;
        /* pointer to V120 config regions (PCIe memory) */
        V120_CONFIG                     *config;
        /* pointer to V120 page descriptor registers (PCIe memory) */
        V120_PD                         *pd;
        /* pointer to V120 IRQ registers (PCIe memory) */
        V120_IRQ                        *irqregs;
        /* pointer to V120's VME monitor block (PCIe memory) */
        struct V120_MONITOR_BLOCK       *mon_blk;
        /* pointer to V120's PCIe record block (PCIe memory) */
        struct V120_PCIE_RECORD_BLOCK   *record_blk;
        /* pointer to memory-mapped base of V120's control region */
        void                            *mapptr;
        /* saved value of errno for this device */
        int                             errnum;
        /* pointer to head of linked list of memory-mapped areas */
        struct vme_mmlist_t             *mmlist;
        /* TODO: meaning of this */
        unsigned long                   flag;
        /*
         * Crate number of V120; set by either v120_next or call to
         * v120_open
         */
        unsigned int                    crateno;
};

#define sDEVNAME_CTRL       "/dev/v120_c"
#define sDEVNAME_VME        "/dev/v120_v"
#define sDEVNAME_IRQ        "/dev/v120_q"
#define N_MALLOCS           512
#define V120_MMAP_OFFS      0
#define V120_MMAP_SIZE      (0x20000U)
#define RECORD_OFFS         (0x14800U)
#define PD_OFFS             (0)
#define CONFIG_OFFS         (0x10000U)
#define MON_OFFS            (0x14000U)
#define IRQ_OFFS            (0x14400U)

#define PAGE_OFFS(idx)    (V120_PAGE_SIZE * (idx))


/* *********************************************************************
 *                      Auxiliary functions
 **********************************************************************/

/*
 * Set errno and save it in the V120's handle.
 *
 * @param h handle to a V120
 * @param err Error number (See man(3) errno)
 */
static void
v120_set_errno(V120_HANDLE *h, int err)
{
        h->errnum = err;
        errno     = err;
}

/*
 * Add the pointer of a mmap'd area to a V120's linked list of mmap'd
 * areas.
 *
 * This allocates memory for a descriptor for this pointer, so the
 * pointer can be munmap'd later.
 *
 * @param h handle to a V120
 * @param ptr pointer to the result of a successful mmap call.
 * @param size size of memory mapped to ptr
 *
 * @return 0 if the pointer and a descriptor for it was added to the
 * list; -1 if a new descriptor cannot be allocated. If the return
 * value is -1, errno will be set to ENOMEM.
 */
static int
v120_add_to_mm_list(V120_HANDLE *h, void *ptr, unsigned long size)
{
        struct vme_mmlist_t *p;
        if ((p = malloc(sizeof(*p))) == NULL)
        {
                v120_set_errno(h, ENOMEM);
                return -1;
        }
        p->ptr  = ptr;
        p->size = size;
        p->next = h->mmlist;

        h->mmlist = p;
        return 0;
}

/*
 * Remove, unmap, and free a pointer p from a V120's linked list of
 * memory-mapped areas.
 *
 * @param h handle to the V120
 * @param p pointer to an item in the V120's linked list of memory
 * map descriptors.
 *
 * @return 0 if there are no bugs. -1 if p is not in the V120's list
 * (a bug).
 */
static int
v120_remove_from_mm_list(V120_HANDLE *h, struct vme_mmlist_t *p)
{
        struct vme_mmlist_t *plist;

        if (h->mmlist == NULL)
        {
                /* List not present. Error. */
                return -1;
        }
        else if (h->mmlist == p)
        {
                /* Remove p from front of list. Most likely. */
                h->mmlist = p->next;
        }
        else
        {
                /* Remove p from middle or end of list. Less likely. */
                for (plist = h->mmlist; plist->next != p;
                     plist = plist->next)
                {
                        if (plist->next == NULL)
                        {
                                /* p not in list. Error. */
                                return -1;
                        }
                }
                plist->next = p->next;
        }

        munmap(p->ptr, p->size);
        free(p);
        return 0;
}

/*
 * Remove, unmap, and free all pointers from a V120's linked list of
 * memory-mapped areas.
 *
 * @param h handle to v120
 *
 * return 0 or -1. If -1 is returned, there is a fatal bug.
 */
static int
v120_clean_mem_list(V120_HANDLE *h)
{
        /* Keep removing top list item until no list left */
        while (h->mmlist != NULL)
        {
                if (v120_remove_from_mm_list(h, h->mmlist) < 0)
                {
                        /* h corrupted */
                        return -1;
                }
        }
        return 0;
}

/*
 * Perform cleanup on a V120 handle.
 *
 * @param h handle to the V120
 *
 * @return 0 or EOF. If EOF is returned, errno will be set to EINVAL.
 * This is because the only conceivable way this function can fail is
 * either a bug in this code (please report if you find one), or user
 * passed a garbage handle to v120_close(), or there is some OS bug.
 */
static int
v120_cleanup(V120_HANDLE *h)
{
        if (v120_clean_mem_list(h) < 0)
        {
                /* Fatal bug error */
                fprintf(stderr, "V120.c runtime bug: Fatal error in function %s: "
                                "unknown state of V120_HANDLE\n", __func__);
                exit(1);
                v120_set_errno(h, EINVAL);
                return EOF;
        }

        if (h->mapptr != NULL)
        {
                munmap(h->mapptr, V120_MMAP_SIZE);
        }

        v120_delete_vme_list(h);

        if (h->fd_irq >= 0)
                close(h->fd_irq);

        close(h->fd_ctrl);
        if (h->fd_vme >= 0)
        {
                close(h->fd_vme);
        }
        free(h);

        return 0;
}

/*
 * Open some device file with read/write and sync flags.
 *
 * @param file name: v120_c or v120_v, eg.
 * @param creatno: 0 to 15, the device minor number.
 *
 * Code common for opening the VME and Control region devices for a
 * V120 whose crate number is crateno.
 *
 * @return The result of the call to open(). (See man(3) open).
 */
static int
v120_open_device(const char *filename, int crateno)
{
        char devname[20];
        int fd;

        snprintf(devname, 19, "%s%u", filename, crateno);
        return open(devname, O_RDWR | O_SYNC);
}

/*
 * Allocate and map npages of a V120's VME space, beginning at pageno.
 *
 * Called from various public functions.
 * @param h handle to the V120
 * @param pageno page number of first page, 0 to V120_PAGE_COUNT - 1
 * @param npabes number of pages to allocate.
 *
 * @return pointer to the base of the allocated and mapped pages, or
 * NULL if there was an error. If the return value is not NULL, this
 * can be used by user; otherwise, errno will be set to ENOMEM.
 */
static void *
v120_alloc_pages(V120_HANDLE *h, int pageno, int npages)
{
#ifdef DEBUG
        printf("Allocating %d pages starting at page %d\n", npages, pageno);
#endif
        if (pageno < 0)
        {
                fprintf(stderr, "BUG: pageno less than zero\n");
                v120_set_errno(h, EINVAL);
                return NULL;
        }
        if (npages < 0)
        {
                fprintf(stderr, "BUG: npages less than zero!\n");
                v120_set_errno(h, EINVAL);
                return NULL;
        }

        void *ptr;
        unsigned long size = V120_PAGE_SIZE * npages;

        if (h->fd_vme < 0)
        {
                h->fd_vme = v120_open_device(sDEVNAME_VME, h->crateno);
                if (h->fd_vme < 0)
                {
                        fprintf(stderr, "V120ALLOCPAGES: V120_OPEN(%s%u) FAILED\n",
                                sDEVNAME_VME, h->crateno);
                        v120_set_errno(h, EBADFD);
                        return NULL;
                }
        }

        ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, h->fd_vme,
                   PAGE_OFFS(pageno));

        if (ptr == NULL || ptr == MAP_FAILED)
        {
                fprintf(stderr, "V120ALLOCPAGES: MMAP FAILED\n");
                goto mapErr;
        }

        if (v120_add_to_mm_list(h, ptr, size) < 0)
        {
                fprintf(stderr, "V120ALLOCPAGES: ADDTOMMLIST FAILED\n");
                munmap(ptr, size);
                goto mapErr;
        }
        return ptr;

mapErr:
        v120_set_errno(h, ENOMEM);
        return NULL;
}

/*
 * Configure a page descriptor.
 *
 * @param hV120 handle ot the V120
 * @param pageno page number to configure
 * @param base 64-bit base address of VME at this page
 * @param config page configuration (see V120_PD macros in V120.h)
 */
static void
v120_configure_pd(V120_HANDLE *hV120, int pageno, uint64_t base,
                  V120_PD config)
{
#ifdef DEBUG
        V120_PD x = (config | ((base / V120_PAGE_SIZE) << 14));
        printf("Configuring page number %d with value %#llX\n", pageno,
                (unsigned long long)x);
#endif

        hV120->pd[pageno] = (config | ((base / V120_PAGE_SIZE) << 14));
}

/*
 * Configure a sequence of consecutive page descriptors with the same
 * basic configuration.
 *
 * @param hV120 Handle to the V120
 * @param startpg page number of the first page, 0 to V120_PAGE_COUNT - 1
 * @param base 64-bit base address of VME at startpg
 * @param config page configuration, not counting VME address. See
 * V120_PD macros in V120.h
 * @param npages Number of pages
 */
static void
v120_configure_pd_range(V120_HANDLE *hV120, int startpg, uint64_t base,
                        V120_PD config, int npages)
{
        int i;
        uint64_t addr;

        for (i = 0; i < npages; i++)
        {
                addr = base + i * V120_PAGE_SIZE;
                v120_configure_pd(hV120, startpg + i, addr, config);
        }
}

/*
 * Return a pointer to the V120 monitor space,
 * without verifying arg.
 */
static volatile V120_MONITOR *
v120_get_monitor_(V120_HANDLE *h, int monitor)
{
        return &h->mon_blk->mon[monitor].mon;
}

/* *********************************************************************
 *                        Public functions
 **********************************************************************/

/**
 * v120_open - open a V120
 * @unit_id:   The ID (0x0 - 0xF) of the V120 to be opened.
 *             This is the same as the number set on the UNIT
 *             switch on the board, and the same as the number
 *             displayed on the unit front panel.
 *
 * @return:  A valid V120_HANDLE * or NULL if the crate cannot be
 *           opened. On a NULL return, errno will be set to indicate
 *           the cause.
 */
V120_HANDLE *
v120_open(int unit_id)
{
        void *ptr;
        V120_HANDLE *h;

        if (unit_id > 15)
        {
                errno = EINVAL;
                goto errArg;
        }

        ptr = calloc(1, sizeof(V120_HANDLE));
        if (!ptr)
        {
                goto errCalloc;
        }
        h = (V120_HANDLE *)ptr;

        h->fd_ctrl = v120_open_device(sDEVNAME_CTRL, unit_id);
        if (h->fd_ctrl < 0)
        {
                /* errno would have been set by open call */
                goto errOpen;
        }

        h->mapptr = mmap(NULL, V120_MMAP_SIZE, PROT_READ | PROT_WRITE,
                         MAP_SHARED, h->fd_ctrl, V120_MMAP_OFFS);
        if (h->mapptr == NULL || h->mapptr == MAP_FAILED)
        {
                goto errMap;
        }

        h->config     = (h->mapptr + CONFIG_OFFS);
        h->irqregs    = (h->mapptr + IRQ_OFFS);
        h->pd         = (h->mapptr + PD_OFFS);
        h->mon_blk    = (h->mapptr + MON_OFFS);
        h->record_blk = (h->mapptr + RECORD_OFFS);
        h->fd_vme     = -1;    /* Do not open h->fd_vme until we're ready for it. */
        h->fd_irq     = -1;    /* Ditto, IRQ */
        h->crateno    = unit_id;

        /* TODO: Initialize stuff. */
        h->list_head = NULL;
        return h;

        /* Start of errors */

        munmap(h->mapptr, V120_MMAP_SIZE);
errMap:
        close(h->fd_ctrl);
errOpen:
        free(h);
errCalloc:
errArg:
        return NULL;
}


/**
 * v120_close - Close a V120.
 * @hV120: Handle to a V120, acquired from &v120_open or &v120_next
 *
 * This will invalidate all pointers to any VME cards controlled
 * by this V120, or to configuration or PD spaces.
 *
 * Note: If another handle to the same V120 is opened, and that handle
 * is unique (EG it was created from its own call to v120_open, rather
 * than a pointer reassignment like 'p2 = p1'), then the other handle
 * may still be used.
 *
 * return:  0 on success, or EOF.  On an EOF return, errno will be
 *          set to indicate the cause.
 */
int
v120_close(V120_HANDLE *hV120)
{
        return v120_cleanup(hV120);
}

/**
 * v120_get_config - Get a pointer to the V120 configuration space.
 * @hV120: Handle to a V120
 *
 * return: Pointer to the V120's configuration space.
 */
V120_CONFIG *
v120_get_config(V120_HANDLE *hV120)
{
        return hV120->config;
}

/**
 * v120_get_irq - Get a pointer to the V120 IRQ control registers
 * @hV120: Handle to a V120
 */
V120_IRQ *
v120_get_irq(V120_HANDLE *hV120)
{
        return hV120->irqregs;
}

/**
 * v120_get_records - Get a pointer to the V120 PCIe monitor space
 * @hV120:      Handle to a V120
 * @record:     Record number, 0 to 127
 *
 * return:      Pointer to a V120_PCIE_RECORDS struct corresponding to
 *              the record.
 */
volatile V120_PCIE_RECORDS *
v120_get_records(V120_HANDLE * hV120, int record)
{
        if (record >= 128)
        {
                errno = EINVAL;
                return NULL;
        }
        return &hV120->record_blk->record[record];
}

/**
 * v120_get_monitor - Get a pointer to the V120 monitor space
 * @hV120:      Handle to a V120
 * monitor:     Monitor number, 0 to 3
 *
 * return: a pointer to the V120 monitor space for the selected monitor.
 */
volatile V120_MONITOR *
v120_get_monitor(V120_HANDLE *hV120, int monitor)
{
        if (monitor >= 4)
        {
                errno = EINVAL;
                return NULL;
        }

        return v120_get_monitor_(hV120, monitor);
}

/* Common to v120_fprint_monitor and v120 snprint_monitor */
#define MONITOR_PRETTY "CTL:%s%s%s%s%s%s%s%s%s\n"               \
                       "MONTRANS:%s%s%s%s%s SRC: %s AM: %d\n"   \
                       "MON_RESP:%s%s%s%s TIMER: %d\n"          \
                       "ADDRESS: 0x%016llX\n"                   \
                       "DATA:    0x%016llX\n"

#define MONITOR_PRINT_ARGS(ctrl, trans, resp, addr, data, transSrc) \
    (((ctrl)  & V120_MON_CTL_IACK   ) != 0) ? "" : " -IACK",    \
    (((ctrl)  & V120_MON_CTL_READ   ) != 0) ? "" : " -READ",    \
    (((ctrl)  & V120_MON_CTL_WRITE  ) != 0) ? "" : " -WRITE",   \
    (((ctrl)  & V120_MON_CTL_RETRY  ) != 0) ? "" : " -RETRY",   \
    (((ctrl)  & V120_MON_CTL_BTO    ) != 0) ? "" : " -BTO",     \
    (((ctrl)  & V120_MON_CTL_BERR   ) != 0) ? "" : " -BERR",    \
    (((ctrl)  & V120_MON_CTL_DTACK  ) != 0) ? "" : " -DTACK",   \
    (((ctrl)  & V120_MON_CTL_OS     ) != 0) ? "" : " -OS",      \
    (((ctrl)  & V120_MON_CTL_ENABLE ) != 0) ? " ENABLED" : " DISABLED",  \
    (((trans) & V120_MON_TRANS_IACK ) != 0) ? " IACK"   : "",   \
    (((trans) & V120_MON_TRANS_WRITE) != 0) ? " WRITE"  : "",   \
    (((trans) & V120_MON_TRANS_LWORD) != 0) ? " LWORD"  : "",   \
    (((trans) & V120_MON_TRANS_DS1  ) != 0) ? " DS1"    : "",   \
    (((trans) & V120_MON_TRANS_DS0  ) != 0) ? " DS0"    : "",   \
    (((transSrc) == V120_MON_TRANS_SRC_PIO) ? "PIO"              \
      : (((transSrc) == V120_MON_TRANS_SRC_COM) ? "COM"          \
          : (((transSrc) == V120_MON_TRANS_SRC_DMA) ? "DMA"      \
            : "UNDEF"))),                                          \
    (unsigned int)V120_MON_TRANS_AM((trans) & V120_MON_TRANS_AM_MASK), \
    (((resp) & V120_MON_RESP_RETRY) != 0) ? " RETRY" : "",      \
    (((resp) & V120_MON_RESP_BTO  ) != 0) ? " BTO"   : "",      \
    (((resp) & V120_MON_RESP_BERR ) != 0) ? " BERR"  : "",      \
    (((resp) & V120_MON_RESP_DTACK) != 0) ? " DTACK" : "",      \
    (resp) & V120_MON_RESP_TIMER_MASK,                              \
    (unsigned long long int)(addr),                               \
    (unsigned long long int)(data)

/**
 * v120_snprint_monitor - Dump a monitor's registers, but in a meaningful
 *                        way, to a buffer.
 * @hV120:    Handle to a V120
 * @monitor:  Selection of a V120's monitor space, 0 to 3
 * @target:   Buffer to copy text
 * @n:        size of buffer, in bytes
 *
 * return: If the monitor number is valid and buffer is large enough to
 * contain text, a pointer to target will be returned.  Otherwise, NULL
 * will be returned and errno will be set to EINVAL.
 */
char *
v120_snprint_monitor(V120_HANDLE * hV120, int monitor, char * target,
                     size_t n)
{
        if (monitor >= 4)
        {
                errno = EINVAL;
                return NULL;
        }

        volatile V120_MONITOR *mon = v120_get_monitor_(hV120, monitor);
        uint32_t ctrl, trans, resp, transSrc;
        uint64_t addr, data;

        ctrl  = mon->mon_ctl;
        trans = mon->mon_trans;
        resp  = mon->mon_resp;
        addr  = mon->addr;
        data  = mon->data;
        transSrc = trans & V120_MON_TRANS_SRC_MASK;
        if (snprintf(target, n, MONITOR_PRETTY,
                     MONITOR_PRINT_ARGS(ctrl, trans, resp, addr, data,
                                        transSrc)) == n)
        {
                return target;
        }
        else
        {
                errno = EINVAL; /* String too small */
                return NULL;
        }
}

/**
 * v120_fprint_monitor - Dump a monitor's registers, but in a meaningful
 *                       way, to a file.
 * @f:       File to write data to; must be open with write permissions.
 * @hV120:   handle to a V120
 * @monitor: Selection of a V120's monitor space, 0 to 3
 *
 * This function is analogous to v120_snprint_monitor
 *
 * return: This function does not return a value, but errno will be set
 * to EINVAL if monitor is incorrect; if the file operation fails, errno
 * will be set by the system.
 */
void
v120_fprint_monitor(FILE * f, V120_HANDLE * hV120, int monitor)
{
    if (monitor >= 4)
    {
            errno = EINVAL;
            return;
    }

    volatile V120_MONITOR *mon = v120_get_monitor_(hV120, monitor);
    uint32_t ctrl, trans, resp, transSrc;
    uint64_t addr, data;

    ctrl  = mon->mon_ctl;
    trans = mon->mon_trans;
    resp  = mon->mon_resp;
    addr  = mon->addr;
    data  = mon->data;
    transSrc = trans & V120_MON_TRANS_SRC_MASK;
    fprintf(f, MONITOR_PRETTY,
            MONITOR_PRINT_ARGS(ctrl, trans, resp, addr, data, transSrc));
}

/**
 * v120_get_pd - Get a pointer to the V120 page descriptor space
 * @hV120:      Handle to a V120
 *
 * return: a pointer to the V120 page descriptor space.
 */
V120_PD *
v120_get_pd(V120_HANDLE *hV120)
{
        return hV120->pd;
}

/**
 * v120_get_all_vme - Get a pointer to the V120 VME space.
 * @hV120:      Handle to a V120
 *
 * return: a pointer to the entire V120 VME space. A failure will return
 * NULL, and set errno.
 */
void *
v120_get_all_vme(V120_HANDLE *hV120)
{
    return v120_alloc_pages(hV120, 0, V120_PAGE_COUNT);
}

/**
 * v120_get_vme - Get a pointer to a subsection of the VME space.
 * @hV120:       Handle to the V120 to be accessed.
 * @start_page:  First page to be mapped.
 * @end_page:    Last page to be mapped.
 *
 * The pointer returned will be valid for the VME space beginning
 * with start_page, and going through end_page.
 *
 * return:  A pointer to the selected space, or NULL for failure.
 *          A NULL return will set errno.
 */
void *
v120_get_vme(V120_HANDLE *hV120, int start_page, int end_page)
{
        if (start_page > end_page)
        {
                /* Need to swap these */
                int start = start_page;
                start_page = end_page;
                end_page = start;
        }

        if (end_page >= V120_PAGE_COUNT)
        {
                /* Not a VME page */
                v120_set_errno(hV120, EINVAL);
                return NULL;
        }

        return v120_alloc_pages(hV120, start_page, end_page - start_page + 1);
}

/**
 * v120_configure_page - Configure a V120 page descriptor.
 * @hV120:   Handle to the V120 to be configured.
 * @idx:     Page descriptor index (0-8191).
 * @base:    Base address for the page.  Must be a multiple of 0x4000.
 * @config:  Access information for the page.
 *
 * return:  A pointer to the configured page on success, or NULL
 *          on error.  A NULL return will set errno.
 */
void *
v120_configure_page(V120_HANDLE *hV120, int idx, uint64_t base,
                    V120_PD config)
{
        if (idx >= V120_PAGE_COUNT || ((base % V120_PAGE_SIZE) != 0))
        {
                v120_set_errno(hV120, EINVAL);
                return NULL;
        }
        v120_configure_pd(hV120, idx, base, config);
        return v120_alloc_pages(hV120, idx, 1);
}

/**
 * v120_get_page_configuration - Get a V120 page descriptor.
 * @hV120:   Handle to the V120 to be queried.
 * @idx:     Page descriptor index (0-8191)
 *
 * return:  The page descriptor requested.  If any of the parameters
 *          are illegal, the return will be 0 and errno will be set.
 */
V120_PD
v120_get_page_configuration(V120_HANDLE *hV120, int idx)
{
        return hV120->pd[idx];
}

/**
 * v120_add_vme_region - Add a VME_REGION to a region list.
 * @hV120: Handle to the V120
 * @data:  Pointer to a VME_REGION structure.
 *
 * warning:     If \b data is on the stack, you should call delete_vme_list()
 *              before destroying data.
 *
 * return:      Pointer to &data if the arguments are valid,
 *              NULL otherwise. If successful, this is a pointer to the
 *              top of hV120's current list of VME regions, which is
 *              terminated by NULL. Using this may be quicker than
 *              v120_get_vme_region, but it is discouraged; the next
 *              field of the list should be be treated as read-only.
 */
VME_REGION *
v120_add_vme_region(V120_HANDLE *hV120, VME_REGION *data)
{
        data->next = hV120->list_head;
        hV120->list_head = data;
        return data;
}

/**
 * v120_get_vme_region - Get info about a VME region.
 * @hV120: Handle to the V120
 * @name:  Enumeration of the VME card.
 *
 * return:      Pointer to the VME card's associated VME region metadata,
 *              or NULL if the region could not be found in the list.
 */
VME_REGION *
v120_get_vme_region(V120_HANDLE *hV120, const char *name)
{
        VME_REGION *pregion;

        pregion = hV120->list_head;
        while (pregion != NULL)
        {
                if (pregion->tag != NULL && !strcmp(pregion->tag, name))
                {
                        return pregion;
                }
                pregion = pregion->next;
        }
        return NULL;
}

/**
 * v120_delete_vme_list - Delete an entire VME_REGION_LIST.
 * @hV120:      Handle to the V120
 *
 * This will not deallocate any data pointed to by the tag or udata
 * fields; if these have been dynamically allocated then they must be
 * freed explicitly before the reference to them is lost.
 */
void
v120_delete_vme_list(V120_HANDLE *hV120)
{
        VME_REGION *head, *next;
        head = hV120->list_head;
        while (head != NULL)
        {
                next = head->next;
                head->next = NULL;
                head = next;
        }
        hV120->list_head = NULL;
}

/**
 * v120_allocate_vme - Configure the accesses for the entire VME crate.
 * @hV120:         A pointer to the V120 to be configured.
 * @start_page:    The lowest number page to allocate.
 *
 * This function will allocate VME pages, starting with start_page and
 * using as many pages as necessary, up to the last available page of
 * 8191.
 *
 * Each item in the VME_REGION_LIST will be modified to set the
 * base, start_page, and end_page fields.
 *
 * return:      A positive number indicating the first unused VME page,
 *              zero indicating that all VME pages were used, or
 *              -1 indicating an error.  In the event of an error
 *              errno will be set.
 */
int
v120_allocate_vme(V120_HANDLE *hV120, unsigned int start_page)
{
        unsigned int pgno, endpage;
        VME_REGION *head = hV120->list_head;

        /* TODO: More thorough argument check */
        if (start_page >= V120_PAGE_COUNT)
        {
                v120_set_errno(hV120, EINVAL);
                return -1;
        }

        pgno = start_page;
        while (head != NULL)
        {
                unsigned int npages;
                unsigned int len;
                unsigned long pgoffs = (head->vme_addr % V120_PAGE_SIZE);

                /* Offset length if the vme_addr is not page-aligned */
                len = head->len + pgoffs;

                npages = (len + V120_PAGE_SIZE - 1) / V120_PAGE_SIZE;
                endpage = pgno + npages - 1;

                head->base = v120_get_vme(hV120, pgno, endpage);
                v120_configure_pd_range(hV120, pgno, head->vme_addr,
                                        head->config, npages);
                if (head->base == NULL)
                {
                        /* errno set in function call */
                        return -1;
                }
                else if (pgoffs != 0)
                {
                        /*
                         * Point to start of _requested_ data. This
                         * doesn't affect cleanup, since we use the
                         * memory list instead.
                         */
                        head->base += pgoffs;
                }

                head->start_page = pgno;
                head->end_page   = pgno + npages - 1;

                pgno += npages;
                head = head->next;
        }
        return endpage + 1;
}

/**
 * v120_next - Close a V120 and open the next available V120, in order of
 *             crate number.
 * @hV120: Pointer to a V120 handle
 *
 * return: Pointer to the V120 with the next lowest available crate
 * number after that associated with hV120. If hV120 is NULL, then the
 * v120 with the lowest available crate number is selected. If no v120
 * can be found with a crate number higher than that associated with
 * hV120, v120_next will return NULL.
 *
 * warning: Ensure any threads using hV120 are complete or terminated
 * before passing it to v120_next as a paramter.
 */
V120_HANDLE *
v120_next(V120_HANDLE * hV120)
{
        int i;
        if (hV120 == NULL)
        {
                i = 0;
        }
        else
        {
                i = hV120->crateno + 1;
                v120_close(hV120);
        }

        /* Paranoid assurance */
        hV120 = NULL;

        while (i < 16)
        {
                hV120 = v120_open(i);
                if (hV120 != NULL)
                {
                        break;
                }
                ++i;
        }

        return hV120;
}

/**
 * v120_irq_open - Get file descriptor to V120 IRQ device
 * @hV120: Pointer to a V120 handle
 *
 * Return: File descriptor to a V120's IRQ device, or -1 if there was
 * an error.  Use this return value for poll(), select(), epoll(), and
 * blocking read() calls.  Do not call close() on this.  Device will be
 * closed with v120_close().
 *
 * Note: This should fail if @hV120 is for a V120 has firmware earlier
 * than revision D, or the v120 driver is an earlier version than 0.5.
 */
int
v120_irq_open(V120_HANDLE *hV120)
{
        if (hV120->fd_irq < 0) {
                char devname[20];
                int res, fd;
                struct flock lock;

                snprintf(devname, 19, "%s%u",
                         sDEVNAME_IRQ, hV120->crateno);
                fd = open(devname, O_RDONLY);
                if (fd < 0)
                        return -1;

                lock.l_type   = F_RDLCK;
                lock.l_whence = SEEK_SET;
                lock.l_start  = 0;
                lock.l_len    = 0x10000;
                res = fcntl(fd, F_SETLK, &lock);
#ifdef DEBUG
                if (res < 0)
                        fprintf(stderr, "set lock failed\n");
#else
                (void)res;
#endif
                hV120->fd_irq = fd;
        }
        return hV120->fd_irq;
}

/**
 * v120_dma_xfr - Transfer DMA
 * @v120: Pointer to a V120 handle
 * @desc: Pointer to the first descriptor in a linked list of DMA
 * transactions.
 *
 * This is a convenience wrapper for a %V120_IOC_DMA_XFR ioctl call.
 *
 * Return: An IOCTL result, zero if successful
 */
int
v120_dma_xfr(V120_HANDLE *v120, struct v120_dma_desc_t *desc)
{
        if (v120->fd_vme < 0) {
                v120->fd_vme = v120_open_device(sDEVNAME_VME, v120->crateno);
                if (v120->fd_vme < 0) {
                        v120_set_errno(v120, EBADFD);
                        return -1;
                }
        }

        return ioctl(v120->fd_vme, V120_IOC_DMA_XFR, desc);
}

/**
 * v120_dma_status - Get DMA status
 * @v120: Pointer to a V120 handle
 * @hdr: Pointer to a struct to store DMA status
 *
 * This is a convenience wrapper for a %V120_IOC_DMA_STATUS ioctl call.
 * It is meant primarily as a debug hook for the V120's developers.
 *
 * Return: An IOCTL result, zero if successful
 */
int
v120_dma_status(V120_HANDLE *v120, struct v120_dma_status_t *status)
{
        if (v120->fd_vme < 0) {
                v120->fd_vme = v120_open_device(sDEVNAME_VME, v120->crateno);
                if (v120->fd_vme < 0) {
                        v120_set_errno(v120, EBADFD);
                        return -1;
                }
        }

        return ioctl(v120->fd_vme, V120_IOC_DMA_STATUS, status);
}

/**
 * v120_persist - (Try to) reconnect to V120 after rebooting/replugging.
 * @v120: Pointer to a V120 handle
 *
 * This tries to reconnect by rewriting the PCI config space with saved
 * values.  Call this at your own risk.
 *
 * Return: Result of %V120_IOC_PERSIST ioctl() call
 */
int
v120_persist(V120_HANDLE *v120)
{
        return ioctl(v120->fd_ctrl, V120_IOC_PERSIST);
}

/**
 * v120_crate - Get the crate number of a V120
 * @v120: Pointer to a V120 handle
 *
 * This is in case you opened with v120_next() and still want the crate
 * number.
 *
 * Return: crate number of @v120
 */
int
v120_crate(V120_HANDLE *v120)
{
        return v120->crateno;
}
