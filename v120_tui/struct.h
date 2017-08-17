/**
 * @file
 * @brief Global struct definitions and a few helper functions/macros
 * @author Paul Bailey
 * @date
 */
#ifndef STRUCT_H
#define STRUCT_H

#include "config.h"
#include <V120.h>
#include <stdarg.h>
#include <stdio.h>
#ifndef HAVE_LIBNCURSES
# define HAVE_LIBNCURSES 0
#endif

struct string_lookup;
struct key_lookup;
struct vme_metadata_t;
struct rnm_t;
struct buffer_t;
struct mode_t;
struct pathlist_t;
#ifndef hash_t
  typedef long int hash_t;
# define hash_t hash_t
#endif

#if 0
/*
 * Functions that look like regular C functions, that can be called
 * by external scripts or keyboard, and can return values.
 * We will not likely require more than four arguments max.
 * There will be wrappers for the scripts, but this provides single-node
 * documentation.
 */
#define DOCFUN(name, doc) \
        const char *doc_##name = doc
#define DEFUN_(name, doc, rtype, ...) \
        DOCFUN(name, doc); \
        rtype name (__VA_ARGS__)
#define DEFUN_0(rtype, name, doc) \
        DEFUN_(name, doc, rtype, void)
#define DEFUN_1(rtype, name, t1, a1, doc)  \
        DEFUN_(name, doc, rtype, t1 a1)
#define DEFUN_2(rtype, name, t1, a1, t2, a2, doc) \
        DEFUN_(name, doc, rtype, t1 a1, t2 a2)
#define DEFUM_3(rtype, name, t1, a1, t2, a2, t3, a3, doc) \
        DEFUN_(name, doc, rtype, t1 a1, t2 a2, t3 a3)
#define DEFUN_4(rtype, name, t1, a1, t2, a2, t3, a3, t4, a4, doc) \
        DEFUN_(name, doc, rtype, t1 a1, t2 a2, t3 a3, t4 a4)
#define DECLFUN_(name, rtype, ...) \
        extern const char *doc_##name;    \
        extern rtype name (__VA_ARGS__)
#define DECLFUN_0(rtype, name) \
        DECLFUN_(name, rtype, void)
#define DECLFUN_1(rtype, name, t1, a1) \
        DECLFUN_(name, rtype, t1 a1)
#define DECLFUN_2(rtype, name, t1, a1, t2, a2) \
        DECLFUN_(name, rtype, t1 a1, t2 a2)
#define DECLFUN_3(rtype, name, t1, a1, t2, a2, t3, a3) \
        DECLFUN_(name, rtype, t1 a1, t2 a2, t3 a3)
#define DECLFUN_4(rtype, name, t1, a1, t2, a2, t3, a3, t4, a4) \
        DECLFUN_(name, rtype, t1 a1, t2 a2, t3 a3, t4 a4)

#endif /* 0 */

/*
 * Declare a public command with this macro.
 *
 * `fn' is the name of the command function (without quotations) and
 * `doc' is the documentation of the function (with quotations), that
 * you want to be visible to the user.
 * 'argname is the name of the parameter of type `struct buffer_t *'.
 * That is just for easier visual identification of structures in your
 * function.
 *
 * CMD_DECL(myfunc, "Execute myfunc");
 */
#define CMD_DECL(fn, doc, argname)  \
        const char *doc_##fn = doc;   \
        void fn (struct buffer_t * argname)
/*
 * Like CMD_DECL, but for file-scope static functions. Not all commands
 * that are visible to the user need to be visible to the rest of the
 * code too.
 */
#define CMD_DECL_PRIVATE(fn, doc, argname) \
        static const char *doc_##fn = doc;   \
        static void fn (struct buffer_t * argname)

/*
 * If your command is in a public function, use this for declaring it
 * in a header.
 */
#define EXT_CMD(fn)                \
        extern const char *doc_##fn; \
        extern void fn (struct buffer_t *)
#define STATIC_CMD(fn) \
        static void fn (struct buffer_t *)

struct string_lookup {
        char *ls_name;
        /* Smr't C doesn't let me declare a constant-at-startup pointer
         * to a constant string. We'll have to dereference a double
         * pointer then. */
        const char **ls_doc;
        void (*ls_fn)(struct buffer_t *);
};

#define SLU_PARAM(nm, fn) \
        { .ls_name = nm, .ls_doc = &doc_##fn, .ls_fn = fn }
#define SLU_END \
        { .ls_name = NULL, .ls_doc = NULL, .ls_fn = NULL }

struct key_lookup {
        int lk_key;
        const char **lk_doc;
        void (*lk_fn)(struct buffer_t *);
};

#define KLU_PARAM(k, fn) \
        { .lk_key = k, .lk_doc = &doc_##fn, .lk_fn = fn }
#define KLU_END \
        { .lk_key = 0, .lk_doc = NULL, .lk_fn = NULL }

struct vme_metadata_t {
        char name[14];
        int sign;
        int dwidth;
};

#define RNM_HASH_SIZE 199 /* prime number twixt 100-200, good enough */

struct rnm_t {
        struct rnm_t *r_next;
        struct vme_metadata_t *r_mdata;
        /* TODO: int *r_hash */
        char *r_path;   /* Saved pathname, for checking if we have cached this already */
        int r_len;
        int r_endi;
        int r_dwidth;
        int r_awidth;
        struct rnm_hash_t {
                char *rh_name;
                hash_t rh_hash;
                struct rnm_hash_t *rh_next;
                int rh_idx; /* Index of r_mdata[] */
        } r_hash[RNM_HASH_SIZE];
        char r_name[14];
};

/*
 * One allocated for each VME region, file browser, and
 * other stuff
 */
struct buffer_t {
        struct buffer_t      *b_next;
        char                 *b_name;   /* This is *not* necessarily unique */
        const struct mode_t  *b_mode;   /* Methods associated with this buffer */
        struct string_lookup *b_slu;
        struct key_lookup    *b_klu;
        struct rnm_t         *b_rnms;
        void                 *b_priv;
        char                 *b_flag;
};

struct mode_t {
        const char *m_name;
        void (*m_left)(struct buffer_t *);
        void (*m_right)(struct buffer_t *);
        void (*m_up)(struct buffer_t *);
        void (*m_dn)(struct buffer_t *);
        void (*m_pgdn)(struct buffer_t *);
        void (*m_pgup)(struct buffer_t *);
        void (*m_end)(struct buffer_t *);
        void (*m_home)(struct buffer_t *);
        void (*m_splash)(struct buffer_t *);
        void (*m_setto)(struct buffer_t *);
        char *(*m_title)(struct buffer_t *);
        void (*m_kill)(struct buffer_t *);
        void (*m_quit)(struct buffer_t *);
};

struct pathlist_t {
        char *p_path;
        struct pathlist_t *p_next;
};

/* Global variables */
extern struct global_t {
        V120_HANDLE *g_v120[16];
        struct rnm_t *g_rnms;             /* list of parsed rnms */
        struct pathlist_t *g_path;        /* list of search paths */
        struct buffer_t *g_buffer;        /* current buffer */
        struct buffer_t *g_lastbuf;       /* last buffer */
        struct buffer_t *g_headbuf;       /* head buffer */
        char *g_home;                     /* Home path */
        FILE *g_instream;                 /* Current input */
        int g_isgui;
} gbl;

/* Helper macros -- we'll only have one `current' buffer at a time */
#define cursleft()     gbl.g_buffer->b_mode->m_left(gbl.g_buffer)
#define cursright()    gbl.g_buffer->b_mode->m_right(gbl.g_buffer)
#define cursup()       gbl.g_buffer->b_mode->m_up(gbl.g_buffer)
#define cursdn()       gbl.g_buffer->b_mode->m_dn(gbl.g_buffer)
#define curspgdn()     gbl.g_buffer->b_mode->m_pgdn(gbl.g_buffer)
#define curspgup()     gbl.g_buffer->b_mode->m_pgup(gbl.g_buffer)
#define cursend()      gbl.g_buffer->b_mode->m_end(gbl.g_buffer)
#define curshome()     gbl.g_buffer->b_mode->m_home(gbl.g_buffer)
#define splash()       gbl.g_buffer->b_mode->m_splash(gbl.g_buffer)

#define modename()     gbl.g_buffer->b_mode->m_name
#define titlename()    gbl.g_buffer->b_mode->m_title(gbl.g_buffer)

/* term.c */
extern volatile int ttnrows_;
extern volatile int ttncols_;

#define ttnrows()       ttnrows_
#define ttncols()       ttncols_
extern void ttstart(void);
extern void ttexit(void);
extern void ttsetfg(int color);
extern void ttsetbg(int color);
extern void ttsize(void);
extern void ttmove(int row, int col);
extern int ttputc(int c);
extern int ttgetc(void);
extern void ttbold(int isbold);
extern void tteeop(void);
extern void tteeol(void);
extern void ttrev(int isrev);
extern void ttto(int ms);
extern void ttcursset(int ms);
extern void ttplain(void);
extern void ttprintf(const char *fmt, ...);
extern void ttvprintf(const char *fmt, va_list ap);
extern void ttopen(void);  /* dummy, for now */
extern void ttflush(void);
extern void ttclose(void); /* dummy, for now */

/* buffer.c */
extern struct buffer_t *buffernew(char *name,
                                  const struct mode_t *pmode,
                                  struct string_lookup *slu,
                                  struct key_lookup *klu,
                                  struct rnm_t *rnm,
                                  void *priv);
extern void bufferlist_start(void);
extern void bufferback(void);
extern void buffernext(void);
extern void buffergoto(struct buffer_t *pbuf);
extern void buffershuffle(void);
extern void bufferkill(struct buffer_t *pbuf);

/* browser.c */
extern struct buffer_t *browsernew(char *name,
                                    VME_REGION *region,
                                    struct rnm_t *metadata,
                                    int crateid);

/* display.c */
#define btnrows()     (ttnrows() - 3)
#define btmove(r, c)  ttmove((r) + 1, c)
extern void modeline(void);
extern void titleprint(const char *title);
extern int mbgetval(const char *prompt, int *pval);
extern void mbclear(void);
extern void mbwarn(const char *fmt, ...);
extern char *mbgets(const char *prompt);

/* controlreg.c */
extern void ctrlnew(V120_HANDLE *h, int crateid);

/* rnm.c */
#define vmebriscomment(c) ((c) == '#')
#define vmebriseol(c)                   \
        ({ typeof(c) c_ = (c);          \
            c_ == '\0' || c_ == '\n'    \
            || c_ == '\r' || vmebriscomment(c_); })
extern char *slide(char *s);
extern char *linestrip(char *s);
extern struct rnm_t *rnmparse(const char *filename);
extern void rnmfree(struct rnm_t *metadata);
extern int rnmsearch(struct rnm_t *rnm, const char *name);

/* helpers.c */
extern hash_t hashstring(const char *s);
extern int editline(char *s,
                    int maxlen,
                    int col,
                    int row,
                    int *cpos,
                    const char *exit);
extern void spaceprint(int row, int col, int len);
extern void highlight(const char *s);
extern char *getfullpath(const char *filemane);
extern FILE *infilesearch(const char *filemane);
extern char *dirstrip(char *name);

/* lookup.c */
extern int keylookupbytable(struct buffer_t *b,
                            struct key_lookup *lut,
                            int c);
#define keylookup(b, c) keylookupbytable(b, b->b_klu, c)
extern int stringlookupbytable(struct buffer_t *b,
                               struct string_lookup *lut,
                               const char *s);
#define stringlookup(b, s) stringlookupbytable(b, b->b_slu, s)

/* input.c */
extern void processinput(int c);
#define iskeyboard() (gbl.g_instream == stdin)
EXT_CMD(inpcommand);

/* stragglers in main.c yt need a home */
extern void add_vme(int crateid,
                    int vmeaddr,
                    int vmelen,
                    const char *rnmfile);
extern void add_dummy(void);
extern int load_file(const char *fname, struct buffer_t *pbuf);

/* quit.c */
extern void quit(void);

/* token.c */
extern int tokenpush(void);
extern void tokenpop(void);
extern void token_start(void);
extern void token_end(void);
extern char *token_next(void);
extern int token_nargs(void);

#if 0
/* TODO: Make this dream come true */
extern float token_next_float(void);
extern int token_next_int(void);
extern double token_next_double(void);
extern long token_next_long(void);
#endif /* 0 */

/* gui.c */
extern int vmebr_start_application(uint16_t *pregs, struct rnm_t *prnm);
extern void notagui(void);

/* Key definitions */
#define TO_CTRL(c)  ((c) & 0x1FU)
#define IS_CTRL(c)  (TO_CTRL(c) == (c))

#if HAVE_LIBNCURSES
# include <ncurses.h>
#else /* !HAVE_LIBNCURSES - need to define these keys */
/*
 * These are typical libncurses values.  All that matters are that they
 * are greater than 8 bits but do not the (1<<31) bit.
 */
#define KEY_LEFT        260  /* left arrow */
#define KEY_RIGHT       261  /* right arrow */
#define KEY_UP          259  /* up arrow */
#define KEY_DOWN        258  /* down arrow */
#define KEY_END         360  /* End key */
#define KEY_HOME        262  /* Home key */
#define KEY_DC          330  /* Forward-delete key */
#define KEY_PPAGE       339  /* Page up */
#define KEY_NPAGE       338  /* Page down */
#define KEY_BACKSPACE   263  /* Backwards delete */

#if 0
# define KEY_ERROR       -1     /* Error reading key */
# define KEY_TIMEOUT     -1     /* No key pressed */
# define KEY_META    (0x8000U)  /* Escape? */
# define TO_META(c)  ((c) | KEY_META)
# define IS_META(c)  (!!((c) & KEY_META) && !((c) & ~(KEY_META | 0xFFU)))
#endif

/*
 * Reminder macros -- in many keyboards, backspace key
 * maps to ASCII ``del'', and forward delete is an escape
 * sequence. To keep both CTRL+H and BACKSPACE options,
 * check a character against both these macros.
 */
#define KEY_DELBACK   0x127

/*
 * Terminal color codes
 */
enum term_color_t {
        COLOR_BLACK = 0,
        COLOR_RED,
        COLOR_GREEN,
        COLOR_YELLOW,
        COLOR_BLUE,
        COLOR_MAGENTA,
        COLOR_CYAN,
        COLOR_WHITE,
        COLOR_DEFAULT = 9,
};

#endif /* !HAVE_LIBNCURSES */

#define KEY_PGUP        KEY_PPAGE
#define KEY_PGDN        KEY_NPAGE

#endif /* STRUCT_H */
