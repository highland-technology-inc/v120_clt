/**
 * @file
 * @brief Buffer handler, and an additional 'top-level' buffer.
 * @author Paul Bailey
 * @date
 */
#include "struct.h"
#include <stdlib.h>
#include <string.h>

static char NONAME[] = { "(Unk)\0" };
static int cpos = 0;

/* Add a newly allocated buffer to the global list */
static void bufferadd2list(struct buffer_t *pbuf)
{
        pbuf->b_next = gbl.g_headbuf;
        gbl.g_headbuf = pbuf;
}

static struct buffer_t *pos2buf(int pos)
{
        struct buffer_t *p = gbl.g_headbuf;
        while (pos > 0) {
                --pos;
                p = p->b_next;
                if (p == NULL)
                        break;
        }
        return p;
}

static void buleft(struct buffer_t *pbuf)
{
        /* Dummy: do nothing */
}

static void buright(struct buffer_t *pbuf)
{
        /* Dummy: do nothing */
}

static void buup(struct buffer_t *pbuf)
{
        if (cpos > 0)
                --cpos;
}

static void budn(struct buffer_t *pbuf)
{
        struct buffer_t *p = pos2buf(cpos);
        if (p != NULL && p->b_next != NULL)
                ++cpos;
}

static void bupgup(struct buffer_t *pbuf)
{
        int n = cpos - btnrows();
        if (n < 0)
                n = 0;

        cpos = n;
}

static void bupgdn(struct buffer_t *pbuf)
{
        int n = btnrows();
        struct buffer_t *p = pos2buf(cpos);

        if (p == NULL)
                return;

        while (p->b_next != NULL && n > 0) {
                ++cpos;
                --n;
                p = p->b_next;
        }
}

static void buend(struct buffer_t *pbuf)
{
        struct buffer_t *p = gbl.g_headbuf;
        int n = 0;
        if (p == NULL) {
                cpos = 0;
                return;
        }
        while (p->b_next != NULL) {
                ++n;
                p = p->b_next;
        }
        cpos = n;
}

static void buhome(struct buffer_t *pbuf)
{
        cpos = 0;
}

static void printabuffer(struct buffer_t *p)
{
        int n = ttncols();
        char *s = p->b_mode->m_title(p);
        while (n > 0 && *s != '\0') {
                ttputc(*s);
                --n;
                ++s;
        }
        /* Redundant in all but one case, but we don't want
         * to print to the end of a line. */
        ttrev(0);
        while (n > 0) {
                ttputc(' ');
                --n;
        }
}

static void bufferemptyline(void)
{
        int n = ttncols();
        while (n > 0) {
                ttputc(' ');
                --n;
        }
}

static void busplash(struct buffer_t *pbuf)
{
        int nr = btnrows();
        int tpos = cpos;
        int i;

        struct buffer_t *p = gbl.g_headbuf;
        while (tpos > nr / 2) {
                --tpos;
                if (p != NULL)
                        p = p->b_next;
        }
        for (i = 0; i < tpos; ++i) {
                if (p != NULL) {
                        btmove(i, 0);
                        printabuffer(p);
                        p = p->b_next;
                } else {
                        bufferemptyline();
                }
        }

        btmove(i, 0);
        ++i;
        if (p != NULL) {
                ttrev(1);
                printabuffer(p);
                ttrev(0);
                p = p->b_next;
        } else {
                bufferemptyline();
        }

        for (; i < nr; ++i) {
                if (p != NULL) {
                        btmove(i, 0);
                        printabuffer(p);
                        p = p->b_next;
                } else {
                        bufferemptyline();
                }
        }
}

static void busetto(struct buffer_t *pbuf)
{
        ttto(-1);
}

static char *butitle(struct buffer_t *pbuf)
{
        return "Buffer list";
}

static void buquit(struct buffer_t *pbuf)
{
        /* Nothing allocated except the buffer itself. */
        bufferkill(pbuf);
}

static void bukill(struct buffer_t *pbuf)
{
        struct buffer_t *p = pos2buf(cpos);
        /* Any two of these checks are redundant, but check them anyway. */
        if (p == gbl.g_headbuf || p->b_mode->m_kill == bukill || p == pbuf) {
                mbwarn("Cannot kill buffer list while program is open");
                return;
        }

        /*
         * In this mode, `kill' means `kill buffer under cursor', not
         * `kill this buffer'.
         */
        p->b_mode->m_kill(p);
}

CMD_DECL_PRIVATE(bugoto, "Display page marked by cursor", pbuf)
{
        struct buffer_t *p = pos2buf(cpos);
        if (p == NULL) {
                mbwarn("No buffers open");
                return;
        }

        buffergoto(p);
}


static const struct mode_t bumode = {
        .m_name    = "buffer list",
        .m_left    = buleft,
        .m_right   = buright,
        .m_up      = buup,
        .m_dn      = budn,
        .m_pgup    = bupgup,
        .m_pgdn    = bupgdn,
        .m_end     = buend,
        .m_home    = buhome,
        .m_splash  = busplash,
        .m_setto   = busetto,
        .m_title   = butitle,
        .m_kill    = bukill,
        .m_quit    = buquit,
};

static struct string_lookup buslu[] = {
        SLU_END,
};

static struct key_lookup buklu[] = {
        KLU_PARAM('\n', bugoto),
        KLU_END
};

/**
 * @brief Kill a buffer.
 * @param Pointer to the buffer to kill and remove from the list
 * @warning Do not pass gbl.g_headbuf as a paramter unless you are
 * a function named 'quit'.
 */
void bufferkill(struct buffer_t *pbuf)
{
        struct buffer_t *p, *q;

        p = gbl.g_headbuf;
        if (pbuf == p) {
                /* Special case: we're removing the head of the
                 * list. */
                gbl.g_headbuf = p->b_next;
                q = p;
                goto remove;
        }

        /* Not the head? Then search the list */
        for (q = p->b_next; q != NULL; p = q, q = p->b_next) {
                if (q == pbuf)
                        break;
        }

        if (q == NULL) {
                /* Another impossibility, but... */
                mbwarn("Cannot find buffer");
                return;
        }

        cpos = 0;  /* Sure, why fuss with it */

        p->b_next = q->b_next;

        if (gbl.g_buffer == q) {
                gbl.g_buffer = p;
                buffergoto(p);
        }

remove:
        /* TODO: Is this all we have to free? */
        free(q);
}

/**
 * @brief Return to previous buffer.
 * @note There is no buffer stack. Only last two buffers may
 * toggle.
 */
void bufferback(void)
{
        buffergoto(gbl.g_lastbuf);
}

/**
 * @brief Go to next buffer
 */
void buffernext(void)
{
        struct buffer_t *p = gbl.g_buffer->b_next;
        if (p == NULL)
                p = gbl.g_headbuf;

        buffergoto(p);
}

/**
 * @brief Create a new buffer containing a list of buffers
 *
 * After this buffer is created, the global list of buffers will
 * contain it and not be NULL. After calling this, then, if the
 * global list of buffers is still NULL, that is justification
 * for panicking and exiting the program.
 */
void bufferlist_start(void)
{
        buffernew("Buffer list", &bumode, buslu, buklu, NULL, NULL);
        cpos = 0;
        gbl.g_buffer = gbl.g_headbuf;
}

/**
 * @brief Create a new buffer and give it parameters
 *
 * @param name Name of the buffer. This might not be unique.
 * @param pmode Methods for the buffer
 * @param slu String lookup table
 * @param klu Keyboard lookup table
 * @param rnm RNM file associated with this buffer. NULL if
 * this is not a browser buffer.
 * @param priv Mode's private data structure. This may be
 * a singleton, or something dynamically allocated and unique
 * to the buffer, depending on the mode. This may be NULL if
 * the buffer does not have a private data structure.
 * @return Pointer to the buffer (which is also now part of
 * the global list of buffers), or NULL if there was a failure.
 * @todo Remove RNM option if it ends up having no use.
 * Ditto re: name.
 */
struct buffer_t *buffernew(char *name,
                            const struct mode_t *pmode,
                            struct string_lookup *slu,
                            struct key_lookup *klu,
                            struct rnm_t *rnm,
                            void *priv)
{
        struct buffer_t *pbuf;
        char *s;

        if ((pbuf = malloc(sizeof(*pbuf))) == NULL)
                return NULL;

        s = malloc(strlen(name) + 1);
        if (s != NULL)
                strcpy(s, name);
        else
                s = NONAME;

        pbuf->b_name = s;
        pbuf->b_mode = pmode;
        pbuf->b_slu  = slu;
        pbuf->b_klu  = klu;
        pbuf->b_rnms = rnm;
        pbuf->b_priv = priv;
        pbuf->b_flag = 0;
        bufferadd2list(pbuf);
        return pbuf;
}

/**
 * @brief Switch to a new buffer
 */
void buffergoto(struct buffer_t *pbuf)
{
        gbl.g_lastbuf = gbl.g_buffer;
        gbl.g_buffer = pbuf;
        tteeop();
        ttplain();
        modeline();
        titleprint(titlename());
        splash();
        ttflush();
        pbuf->b_mode->m_setto(pbuf);
}

/**
 * @brief Swap the first two buffers in the global buffer list.
 *
 * This should be called every time a new buffer is added after the
 * buffer list has been initialized.
 */
void buffershuffle(void)
{
        register struct buffer_t *b, *c;

        b = gbl.g_headbuf;
        if (b != NULL && b->b_next != NULL) {
                /* Carefully now ... */
                c = b->b_next;
                b->b_next = c->b_next;
                c->b_next = b;
                gbl.g_headbuf = c;
        }
}

