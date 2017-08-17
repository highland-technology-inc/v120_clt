#ifndef V120_CLT_COMMON_H
#define V120_CLT_COMMON_H


/* XXX: Should this be in the library header instead? */
#if HAVE_STDINT_H
# include <stdint.h>
#elif HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#if HAVE_CONFIG_H
# include <config.h>
#else
# define PACKAGE_BUGREPORT  "pbailey@highlandtechnology.com"
# define PACKAGE_NAME       "v120"
# define PACKAGE_VERSION    "[NOT VERSIONED, built on " __DATE__ "]"
#endif

#include <V120.h>

/* not used in this header, but optarg is in all the sources */
#include <unistd.h>

struct v120_args_t {
        int crate;
        unsigned long awidth;
        unsigned long dwidth;
        int split;
        int binary;
        int vmeprint;
        int fpga;
        int fair;
        unsigned long speed;
        int slot0;
        int verbose;
};

/* values of .dwidth field in v120_args_t (NOT a V120_PD type) */
enum vme_dwidth_t {
        VME_D8EO = 0,
        VME_D8O,
        VME_D16,
        VME_D32,
        VME_NDWIDTHS,
};

/* v120_macro_write() values */
#define MACRO_FLST_CMD          (0x00000081U)
#define MACRO_RESET_CMD         (0x00000082U)
#define MACRO_ERASE_CMD         (0x00000083U)
#define MACRO_REFLASH_CMD       (0x00000084U)
#define MACRO_UNLOCK_CMD        (0x00000086U)
#define MACRO_IDENT_CMD         (0x00000087U)
#define MACRO_FPGA_RESET_CMD    (0x000000A5U)

/* do_for_each_crate() flags */
#define DFEC_INTERM             (1U << 0)
#define DFEC_NOTALL             (1U << 1)

#define REFLASH_BUFSIZE     1024U
#define REFLASH_ARRAYLEN    (REFLASH_BUFSIZE / sizeof(uint32_t))

/* common.c */
extern void v120_perror_(const char *msg, ...);
#define v120_perror(S, args...) v120_perror_("v120: " S, ## args)
#define BUG() v120_perror("BUG: %s line %d", __FUNCTION__, __LINE__)
extern int do_for_each_crate(const struct v120_args_t *args,
                             int (*fn)(V120_HANDLE *, void *),
                             void *priv, unsigned int flags);
extern int v120_macro_wait(V120_HANDLE *v120, V120_CONFIG *cfg_regs);
extern void v120_macro_write(V120_HANDLE *v120,
                             V120_CONFIG *cfg_regs, uint32_t val);

/* report.c */
extern int v120_report_flash(int argc, char **argv, const struct v120_args_t *args);
extern int v120_report_ident(int argc, char **argv, const struct v120_args_t *args);
extern int v120_report_power(int argc, char **argv, const struct v120_args_t *args);
extern int v120_report_uptime(int argc, char **argv, const struct v120_args_t *args);
extern int v120_report_status(int argc, char **argv, const struct v120_args_t *args);
extern int v120_report_monitor(int argc, char **argv, const struct v120_args_t *args);
extern int v120_report_pcie(int argc, char **argv, const struct v120_args_t *args);

/* rw.c */
extern int v120_write(int argc, char **argv, const struct v120_args_t *args);
extern int v120_read(int argc, char **argv, const struct v120_args_t *args);

/* reset.c */
extern int v120_reset(int argc, char **argv, const struct v120_args_t *args);
extern int v120_sysreset(int argc, char **argv, const struct v120_args_t *args);

/* flash.c */
extern int v120_flash_upgrade(int argc, char **argv, const struct v120_args_t *args);
extern int v120_lflash_upgrade(int argc, char **argv, const struct v120_args_t *args);

/* requester.c */
extern int v120_requester(int argc, char **argv, const struct v120_args_t *args);

/* scan.c */
extern int v120_scan(int argc, char **argv, const struct v120_args_t *args);

/* help.c */
extern const char *v120_help_string;

#endif /* V120_CLT_COMMON_H */
