/* report.c - Smaller "v120 report" subcommands */
#include "v120_common.h"
#include <stdio.h>
#include <stdlib.h>

static void
print_one_ps(const char *name, uint32_t v)
{
        printf("%8s: %-6hd AVE  %-6hd PP\n",
               name, v & 0xFFFFU, (v >> 16) & 0xFFFFU);
}

static int
print_power(V120_HANDLE *v120, void *unused)
{
        V120_CONFIG *cfg_regs = v120_get_config(v120);
        printf("Temp:     %.1f\n", (double)(cfg_regs->temperature) / 10.0);
        printf("Airflow:  %u\n",  cfg_regs->airflow   & 0xFFFFU);
        print_one_ps("+5", cfg_regs->psP5Vme);
        print_one_ps("+12", cfg_regs->psP12Vme);
        print_one_ps("-12", cfg_regs->psN12Vme);
        print_one_ps("+24", cfg_regs->psP24Vxi);
        print_one_ps("-24", cfg_regs->psN24Vxi);
        print_one_ps("+3.3", cfg_regs->psP3_3Vxi);
        print_one_ps("-2", cfg_regs->psN2Vxi);
        print_one_ps("-5", cfg_regs->psN5_2Vxi);
        print_one_ps("+1.2", cfg_regs->psP1_2Int);
        print_one_ps("+2.5", cfg_regs->psP2_5Int);
        print_one_ps("+3.3", cfg_regs->psP3_3Int);
        return EXIT_SUCCESS;
}

static int
print_uptime(V120_HANDLE *v120, void *unused)
{
        V120_CONFIG *cfg_regs = v120_get_config(v120);
        printf("%u\n", cfg_regs->uptime);
        return EXIT_SUCCESS;
}

#define STATUS_UA (1UL << 22)
#define STATUS_DH (1UL << 18)
#define STATUS_LF (1UL << 17)
#define STATUS_EA (1UL << 16)
#define STATUS_PE (1UL << 15)
#define STATUS_FE (1UL << 14)
#define STATUS_TE (1UL << 13)
#define STATUS_UF (1UL << 12)
#define STATUS_DF (1UL << 11)
#define STATUS_VX (1UL << 1)
#define STATUS_UG (1UL << 0)

static int
print_one_status(V120_HANDLE *v120, void *unused)
{
        V120_CONFIG *cfg_regs = v120_get_config(v120);
        uint32_t status = cfg_regs->status;

        printf("Status value: 0x%08X\n", status);
        printf("\tUSB %s\n",
               !!(status & STATUS_UA) ? "connected" : "disconnected");
        printf("\tIP %s\n", !!(status & STATUS_DH) ? "DHCP" : "static");
        printf("\tEthernet link %s%s\n",
               ((status & STATUS_EA) != 0) ? "active" : "not active",
               ((status & STATUS_LF) != 0) ? " 100Mbps" : "");
        if ((status & STATUS_PE) != 0)
                printf("\tPower supply failure\n");
        if ((status & STATUS_FE) != 0)
                printf("\tAir flow warning\n");
        if ((status & STATUS_TE) != 0)
                printf("\tTemperature warning\n");
        if ((status & STATUS_UF) != 0)
                printf("\tFirmware upgrade present but failed\n");
        if ((status & STATUS_DF) != 0)
                printf("\tFirmware is a draft version\n");
        printf("\tBoard is %s\n", !!(status & STATUS_VX) ? "VXI" : "VME");
        if ((status & STATUS_UG) != 0)
                printf("\tFirmware upgrade is running\n");

        return EXIT_SUCCESS;
}

/* helper for print_one_id() */
static int
fpgaid(V120_HANDLE *v120, V120_CONFIG *cfg_regs, uint32_t *id)
{
        int ret;

        v120_macro_write(v120, cfg_regs, MACRO_IDENT_CMD);
        ret = v120_macro_wait(v120, cfg_regs);
        if (!ret)
                *id = ((cfg_regs->mp[ 0 ] & 0xFFFF0000U) >> 16);
        return ret;
}

static int
print_one_id(V120_HANDLE *v120, void *unused)
{
        uint32_t fpgarev;
        V120_CONFIG *cfg_regs = v120_get_config(v120);
        int idresult = fpgaid(v120, cfg_regs, &fpgarev);

        /* R.T.F.M. */
        printf("Crate ID: %u\n", (unsigned int)(cfg_regs->dips & 0xFU));
        printf("Manufacturer ID: 0x%08X\n", (unsigned int)cfg_regs->mfr_id);
        printf("Module type: %u\n", (unsigned int)cfg_regs->modtype);
        printf("Hardware Revision: %c\n", (int)cfg_regs->modrev);
        printf("Firmware Revision: %c\n", (int)cfg_regs->rom_rev & 0x7FU);
        printf("Firmware Draft: %u\n", (cfg_regs->rom_rev & 0xFFFF0000U) >> 16);
        if (!idresult) {
                printf("FPGA Revision: %c\n", (int)fpgarev & 0x7FU);
                printf("FPGA Draft: %u\n", (fpgarev >> 8) & 0xFFU);
        } else {
                printf("FPGA Revision: unavailable\n");
                printf("FPGA Draft: unavailable\n");
        }
        printf("Serial number: %u\n", (unsigned int)cfg_regs->serial);
        printf("Dash number: %u\n", (unsigned int)cfg_regs->dash);
        return EXIT_SUCCESS;
}

static int
print_one_mon(V120_HANDLE *v120, void *priv)
{
        int i;
        int monno = *(int *)priv;
        for (i = monno < 0 ? 4 : 1; i > 0; i--, monno++) {
                printf("Monitor number %d\n", monno);
                v120_print_monitor(v120, monno);
                putchar('\n');
        }
        return EXIT_SUCCESS;
}

#define STATUS_TO_NS(status) \
    ((((status) & V120_PCIE_MON_STATUS_TIMER_MASK) >> V120_PCIE_MON_STATUS_TIMER_LSB) * 8)

#define STATUS_TO_BURST_COUNT(status) \
    (((status) & V120_PCIE_MON_STATUS_COUNT_MASK) >> V120_PCIE_MON_STATUS_COUNT_LSB)

/* helper to print_pcie_records() */
static int
print_one_pcie_record(volatile V120_PCIE_RECORDS *p, int idx)
{
        static const uint32_t MONITOR_START = 0x14800;
        static const uint32_t MONITOR_END
                       = MONITOR_START + sizeof(V120_PCIE_RECORDS) * 128;
        uint32_t status  = p->status;
        uint32_t address = p->address;

        if (!(status & V120_PCIE_MON_STATUS_VALID)
            || (address >= MONITOR_START && address < MONITOR_END)) {
                return 1;
        }

        printf("%3d:\n----\nSTATUS: %s bar %c %s\n", idx,
                !!(status & V120_PCIE_MON_STATUS_READ)  ? "read"  : "write",
                !!(status & V120_PCIE_MON_STATUS_BAR)   ? '1'     : '0',
                !!(status & V120_PCIE_MON_STATUS_START) ? "Start of burst" : "");
        printf("TIME: %u ns\n", STATUS_TO_NS(status));
        printf("BURST COUNT: %u\n", STATUS_TO_BURST_COUNT(status));
        printf("BYTE EN: 0x%02X\n", (status & V120_PCIE_MON_STATUS_BYTE_EN_MASK));
        printf("ADDRESS: 0x%08X\n", (unsigned int)address);
        printf("DATA: 0x%016llX\n", (unsigned long long)p->data);

        return 0;
}

static int
print_pcie_records(V120_HANDLE *v120, void *unused)
{
        volatile V120_PCIE_RECORDS *p;
        int i;

        /* Dump all */
        for (i = 0; i < 128; ++i) {
                p = v120_get_records(v120, i);
                if (print_one_pcie_record(p, i))
                        break;
                putchar('\n');
        }

        /* reset */
        p = v120_get_records(v120, 0);
        p->status = 0xFFFFFFFFU;

        return EXIT_SUCCESS;
}

static int
v120_report_flash_(V120_HANDLE *v120, void *unused)
{
        V120_CONFIG *cfg_regs;
        int i;
        uint8_t *src;

        cfg_regs = v120_get_config(v120);
        if (!cfg_regs) {
                v120_perror("cannot get config regs");
                return EXIT_FAILURE;
        }
        printf("Unlocking flash\n");
        v120_macro_write(v120, cfg_regs, MACRO_FLST_CMD);
        if (v120_macro_wait(v120, cfg_regs) < 0) {
                v120_perror("flash status macro fail");
                return EXIT_FAILURE;
        }
        src = (uint8_t *)cfg_regs->buf;
        for (i = 0; i < REFLASH_BUFSIZE; ++i) {
                int c = *src++;
                if (c == '\0')
                        break;
                putchar(c);
        }
        putchar('\n');
        return EXIT_SUCCESS;
}

int
v120_report_power(int argc, char **argv, const struct v120_args_t *args)
{
        return do_for_each_crate(args, print_power, NULL, 0);
}

int
v120_report_uptime(int argc, char **argv, const struct v120_args_t *args)
{
        return do_for_each_crate(args, print_uptime, NULL, 0);
}

int
v120_report_ident(int argc, char **argv, const struct v120_args_t *args)
{
        return do_for_each_crate(args, print_one_id, NULL, 0);
}

int
v120_report_status(int argc, char **argv, const struct v120_args_t *args)
{
        return do_for_each_crate(args, print_one_status, NULL, 0);
}

int
v120_report_monitor(int argc, char **argv, const struct v120_args_t *args)
{
        int mon;
        char *endptr;

        if (optind >= argc) {
                mon = -1;
        } else {
                mon = strtoul(argv[optind], &endptr, 0);
                if (mon > 3 || endptr == argv[optind] || errno) {
                        v120_perror("Invalid monitor '%s'", argv[optind]);
                        return EXIT_FAILURE;
                }
        }
        return do_for_each_crate(args, print_one_mon, &mon, 0);
}

int
v120_report_pcie(int argc, char **argv, const struct v120_args_t *args)
{
        return do_for_each_crate(args, print_pcie_records, NULL, DFEC_NOTALL);
}

int
v120_report_flash(int argc, char **argv, const struct v120_args_t *args)
{
        return do_for_each_crate(args, v120_report_flash_, NULL, 0);
}
