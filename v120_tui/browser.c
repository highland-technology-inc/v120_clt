/**
 * @file
 * @brief Methods for browser mode
 *
 * @author Paul Bailey
 * @date
 */

#include "struct.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * struct browser_t - the b_priv of a browser type of buffer
 * @br_sel:       Register number ("") relative to br_offs
 * @br_refresh:   Screen refresh rate, in milliseconds
 * @br_offs:      Register number (size based on br_dwidth) of first
 *                register on page
 * @br_ncols:     Constantly dependent on br_dwidth
 * @br_dwidth:    Data width. Initially same as RNM, but user can change
 *                it.
 * @br_printlen:  Print length of single reg. val.
 * @br_strbar:    To print on title bar
 * @br_dwshift:   Correction in case user changes datawidth
 * @br_lastwrite: Value most recently written to a register
 * @br_topoffs:   Top byte offset, determined from RNM file
 * @br_crateid:   Crate ID, or neg. if dummy buffer
 * @br_region:    See &V120_REGION
 * @br_mdata:     RNM metadata associated with this struct
 */
struct browser_t {
        int br_sel;
        int br_refresh;
        int br_offs;
        int br_ncols;
        int br_dwidth;
        int br_printlen;
        char *br_strbar;
        int br_dwshift;
        int br_lastwrite;
        int br_topoffs;
        int br_crateid;
        VME_REGION *br_region;
        struct rnm_t *br_mdata;
};

/* Deuglication */
#define br_pregs br_region->base
#define br_size  br_region->len
#define TAG_STRLEN 10
#define DUMP_COL   6
#define ROW_TAG    2
#define TO_TAGS(br)           \
        (DUMP_COL + TAG_STRLEN \
          + (br)->br_printlen * (br)->br_ncols)
#define BROWSER_MS      50 /* default */
#define REFRESH_MIN_MS  10

/* `struct buffer' to its `struct browser' */
#define BROWSER(buf)  ((struct browser_t *)((buf)->b_priv))

/*
 * Generic IO functions for VME (IE not necessecarily V120_CONFIG, ergo
 * not a struct hard-coded in this program)
 *
 * XXX: These should be moved to a more public header
 */

/*
 * Get a pointer to a VME register, given the base pointer, the
 * correctly-aligned offset, and the type
 */
#define REGPTR(pvbase, byteoffs, type) \
        ((volatile type *)((void *)(pvbase) + (byteoffs)))

#define WRITE_REG_(pvbase, byteoffs, val, type_) \
        do { (*REGPTR(pvbase, byteoffs, type_)) = (val); } while (0)

/*
 * Write to VME registers of a certain type, given their base pointer,
 * an correctly-aligned offset, and the value to write.
 */
#define WRITE_REG_16(pvbase, byteoffs, val) \
        WRITE_REG_(pvbase, byteoffs, val, uint16_t)
#define WRITE_REG_32(pvbase, byteoffs, val) \
        WRITE_REG_(pvbase, byteoffs, val, uint32_t)


/*
 * Return the number of VME registers that can be displayed on one page.
 * This is not constant, since a young techie brat may change the size of
 * the terminal at will.
 */
static int brarraylen(struct browser_t *br)
{
        return (btnrows() - ROW_TAG) * br->br_ncols;
}

/*
 * Get the highest register number that can be displayed at the start of
 * a page.
 */
static int broffmax(struct browser_t *br)
{
        return br->br_region->len / br->br_dwidth - brarraylen(br);
}

/* Move cursor to beginning of VME map */
static void brstartpos(struct browser_t *br)
{
        br->br_sel = 0;
        br->br_offs = 0;
}

/* Move cursor to end of VME map */
static void brendpos(struct browser_t *br)
{
        br->br_sel = brarraylen(br) - 1;
        br->br_offs = broffmax(br);
}

/*
 * Create a title string. Free it with free().
 */
static char *titlemake(const char *name, int vmeaddr, int crateid)
{
        int count;
        char buf[80];
        int n = sizeof(buf);
        char *s = buf;

        count = snprintf(s, n, "%s ", name);
        n -= count;
        s += count;
        if (n < 0)
                goto done;

        if (vmeaddr < 0)
                count = snprintf(s, n, "CONFIG ");
        else
                count = snprintf(s, n, "0x%08X ", vmeaddr);

        n -= count;
        s += count;
        if (n < 0)
                goto done;

        if (crateid < 0)
                snprintf(s, n, "No crate");
        else
                snprintf(s, n, "Crate %d", crateid);

done:
        buf[sizeof(buf) - 1] = '\0';
        return strdup(buf);
}

/*
 * Make sure highlighted selection is not off the display page or
 * off the VME map
 */
static void brcheckpos(struct browser_t *br)
{
        register int sel = br->br_sel;
        register int off = br->br_offs;
        register int ncols = br->br_ncols;

        if (sel < 0) {
                sel += ncols;
                off -= ncols;
        } else if (sel >= brarraylen(br)) {
                sel -= ncols;
                off += ncols;
        }

        br->br_offs = off;
        br->br_sel  = sel;

        if (off < 0)
                brstartpos(br);
        else if (off > broffmax(br))
                brendpos(br);
}

static void brleft(struct buffer_t *pbuf)
{
        struct browser_t *pbr;

        pbr = BROWSER(pbuf);
        pbr->br_sel--;
        brcheckpos(pbr);
}

static void brright(struct buffer_t *pbuf)
{
        struct browser_t *pbr;

        pbr = BROWSER(pbuf);
        pbr->br_sel++;
        brcheckpos(pbr);
}

static void brup(struct buffer_t *pbuf)
{
        struct browser_t *pbr;

        pbr = BROWSER(pbuf);
        pbr->br_sel -= pbr->br_ncols;
        brcheckpos(pbr);
}

static void brdn(struct buffer_t *pbuf)
{
        struct browser_t *pbr;

        pbr = BROWSER(pbuf);
        pbr->br_sel += pbr->br_ncols;
        brcheckpos(pbr);
}

static void brpgdn(struct buffer_t *pbuf)
{
        struct browser_t *pbr;

        pbr = BROWSER(pbuf);
        pbr->br_offs += brarraylen(pbr);
        brcheckpos(pbr);
}

static void brpgup(struct buffer_t *pbuf)
{
        struct browser_t *pbr;

        pbr = BROWSER(pbuf);
        pbr->br_offs -= brarraylen(pbr);
        brcheckpos(pbr);
}

static void brend(struct buffer_t *pbuf)
{
        brendpos(BROWSER(pbuf));
}

static void brhome(struct buffer_t *pbuf)
{
        brstartpos(BROWSER(pbuf));
}

/* TODO: General form of VME write, CONFIG_REG write */
static void brwrite1(struct browser_t *br, unsigned int v)
{
        if (br->br_dwidth == 2)
                ((uint16_t *)br->br_pregs)[br->br_sel + br->br_offs] = (uint16_t)v;
        else
                ((uint32_t *)br->br_pregs)[br->br_sel + br->br_offs] = (uint32_t)v;
}

CMD_DECL_PRIVATE(brditto,
        "Repeat last register write on the currently selected register",
        pbuf)
{
        struct browser_t *pbr;

        if (iskeyboard()) {
                /* Don't try this from a file */
                pbr = BROWSER(pbuf);
                brwrite1(pbr, pbr->br_lastwrite);
        }
}

CMD_DECL_PRIVATE(brwrite,
        "Write a value to the current cursor. The minibuf will show "
        "either a `H' or a `T' on the right side to indicate number "
        "base (hexadecimal or default decimal). The default is hex.",
        pbuf)
{
        int v, r;
        struct browser_t *pbr;

        /* This command only has meaning for the keyboard */
        if (!iskeyboard())
                return;

        pbr = BROWSER(pbuf);

        ttto(-1);
        r = mbgetval("Enter value: ", &v);
        pbuf->b_mode->m_setto(pbuf);

        if (!r) {
                brwrite1(pbr, v);
                pbr->br_lastwrite = v;
        }
}

/*
 * Finisher for brgoto and brgotoname.
 * v is assumed to be a safely checked byte offset.
 */
static void brjump(struct browser_t *pbr, int v)
{
        v /= pbr->br_dwidth;
        pbr->br_offs = v & ~0xFU;
        pbr->br_sel  = v & 0xFU;

        while (pbr->br_offs > broffmax(pbr)) {
                pbr->br_offs -= pbr->br_ncols;
                pbr->br_sel  += pbr->br_ncols;
        }
        /* assert sel < arraylen */
}

CMD_DECL_PRIVATE(brgotoname,
        "Move cursor to the first register with a given name. The name "
        "must match a name in the RNM file associated with this card. "
        "If there is no match, or if the card does not have an RNM file "
        "associated with it, there will be no jump.",
        pbuf)
{
        char *s;
        int offs;
        struct browser_t *pbr;

        /* This command only has meaning for the keyboard */
        if (!iskeyboard())
                return;

        if (pbuf->b_rnms == NULL) {
                mbwarn("No name info for this card");
                return;
        }

        ttto(-1);
        s = mbgets("Enter name of register (must match RNM): ");
        pbuf->b_mode->m_setto(pbuf);

        if (s != NULL) {
                pbr = BROWSER(pbuf);
                offs = rnmsearch(pbuf->b_rnms, s);
                free(s);
                if (offs < 0)
                        mbwarn("Could not find register in this card");
                else
                        brjump(pbr, offs * pbr->br_dwidth);
        }
}

CMD_DECL_PRIVATE(brwrite2name,
        "Write to a register whose name matches the argument, "
        "or at the offset provided if the name argument is a number."
        "This is SLOW and only written for the sake of development; "
        "some day this stuff will be cached, hashed, and bundled so "
        "that a program can be loaded and then run quickly later.",
        pbuf)
{
        char *s;
        char *ts;
        int offs;
        int v;
        int r;
        void *pregs = BROWSER(pbuf)->br_pregs;
        if (iskeyboard()) {
                s = mbgets("Enter name or offset of register: ");
                if (s == NULL)
                        return;

                if ((r = mbgetval("Enter value: ", &v)) != 0) {
                        free(s);
                        return;
                }
        } else {
                if ((s = token_next()) == NULL) {
                        mbwarn("Expected: offset");
                        return;
                }
                if ((ts = token_next()) == NULL) {
                        mbwarn("Expected: write value");
                        return;
                }
                v = strtol(ts, NULL, 0);
        }

        if (isdigit(*s)) {
                offs = strtol(s, NULL, 0);
        } else {
                offs = rnmsearch(pbuf->b_rnms, s);
                offs *= BROWSER(pbuf)->br_dwidth;
        }

        if (iskeyboard())
                free(s);

        if (offs >= 0) {
                if (pbuf->b_rnms->r_dwidth == 2) {
                        offs &= ~1U;
                        WRITE_REG_16(pregs, offs, v & 0xFFFFU);
                } else {
                        offs &= ~3U;
                        WRITE_REG_32(pregs, offs, v);
                }
        }
}


CMD_DECL_PRIVATE(brgoto,
        "Move cursor to a register at a specified byte offset",
        pbuf)
{
        int v, r;
        struct browser_t *pbr;

        if (iskeyboard()) {
                /* This function is available from keyboard only */
                pbr = BROWSER(pbuf);

                ttto(-1);
                r = mbgetval("Enter position: ", &v);
                pbuf->b_mode->m_setto(pbuf);

                if (!r && v < pbr->br_topoffs && v >= 0)
                        brjump(pbr, v);
        }
}

static char *brtitle(struct buffer_t *pbuf)
{
        struct browser_t *pbr;
        char *ret;

        pbr = BROWSER(pbuf);
        if (pbr->br_strbar != NULL)
                ret = pbr->br_strbar;
        else
                ret = NULL;

        return ret;
}

static int idxchk(struct browser_t *pbr, int i)
{
        if (pbr->br_dwshift == 0)
                return i;
        else if (pbr->br_dwshift > 0)
                return i >> pbr->br_dwshift;
        else
                return i << -pbr->br_dwshift;
}

static void brsplash(struct buffer_t *pbuf)
{
        static const char *OVERLAY[] = {
                "  00    02    04    06    08    0A    0C    0E  ",
                "    00        04        08        0C      ",
        };
        static const char *BRTAGS[] = {
                "Name:",
                "Offset:",
                "UNS16 L:",
                "UNS16 H:",
                "INT16 L:",
                "INT16 H:",
                "UNS32:",
                "INT32:",
                "Hex:",
                "Float:",
                NULL,
        };
        static const int ROW_START = 2;

        struct browser_t *pbr;
        int row;
        int column;
        int drawno = 0;
        int nrows;
        int ncols;
        void *input;
        unsigned int v = 0;
        unsigned int idx;
        int dwidth;
        int pdi;
        int byteoff;
        const char **ptags;
        volatile float fv = 0.;

        pbr = BROWSER(pbuf);
        input = pbr->br_pregs + (pbr->br_offs * pbr->br_dwidth);
        nrows = btnrows() - ROW_START;
        ncols = pbr->br_ncols;
        dwidth = pbr->br_dwidth;

        btmove(0, 0);
        ttprintf("\n");
        btmove(1, DUMP_COL);
        ttprintf("%s", dwidth == 2 ? OVERLAY[0] : OVERLAY[1]);

        for (row = 0; row < nrows; ++row) {
                ttrev(0);
                ttbold(0);
                btmove(row + ROW_START, 0);
                ttprintf(" %04X ",
                          (pbr->br_offs
                            + ncols * row) * dwidth);

                for (column = 0; column < ncols; ++column) {
                        if (drawno == pbr->br_sel) {
                                ttrev(1);
                                if (dwidth == 2) {
                                        v = *(uint16_t *)input;
                                } else {
                                        v = *(uint32_t *)input;
                                        fv = *(float *)input;
                                }
                        } else if (drawno - 1 == pbr->br_sel) {
                                ttrev(0);
                        }

                        pdi = idxchk(pbr, drawno + pbr->br_offs);
                        if (pbr->br_mdata != NULL && pdi < pbr->br_mdata->r_len) {
                                if (pbr->br_mdata->r_mdata[pdi].name[0] != '\0')
                                        ttbold(1);
                                else
                                        ttbold(0);
                        } else {
                                ttbold(0);
                        }

                        ttputc(' ');
                        if (dwidth == 2)
                                ttprintf("%04hX", (*(unsigned short *)input));
                        else if (dwidth == 4)
                                ttprintf("%08X", (*(unsigned int *)input));

                        ttputc(' ');

                        input += pbr->br_dwidth;
                        ++drawno;
                }
        }
        ttplain();
        column = TO_TAGS(pbr);
        for (ptags = BRTAGS, row = ROW_TAG; *ptags != NULL; ++ptags, ++row) {
                btmove(row, column);
                ttprintf("%s", *ptags);
        }
        column += TAG_STRLEN;
        btmove(ROW_TAG, column);
        idx = idxchk(pbr, pbr->br_sel + pbr->br_offs);
        if (pbr->br_mdata != NULL && idx < pbr->br_mdata->r_len) {
                ttbold(1);
                ttprintf("%-12s", pbr->br_mdata->r_mdata[idx].name);
                ttbold(0);
        }
        btmove(ROW_TAG + 1, column);
        byteoff = idx * pbr->br_dwidth;
        if (pbr->br_dwshift < 0)
                byteoff >>= -pbr->br_dwshift;
        else if (pbr->br_dwshift > 0)
                byteoff <<= pbr->br_dwshift;

        ttprintf("%04X", byteoff);
        btmove(ROW_TAG + 2, column);
        ttprintf("%-6u", v & 0xFFFFU);
        btmove(ROW_TAG + 3, column);
        ttprintf("%-6u", (v & 0xFFFF0000U) >> 16);
        btmove(ROW_TAG + 4, column);
        ttprintf("%-6hd", (short)(v & 0xFFFFU));
        btmove(ROW_TAG + 5, column);
        ttprintf("%-6hd", (short)((v & 0xFFFF0000U) >> 16));
        btmove(ROW_TAG + 6, column);
        ttprintf("%-11u", v);
        btmove(ROW_TAG + 7, column);
        ttprintf("%-11d", (int)v);
        btmove(ROW_TAG + 8, column);
        ttprintf("%08X", v);
        btmove(ROW_TAG + 9, column);
        ttprintf("%.8f", (double)fv);
        if (pbr->br_dwshift != 0) {
                btmove(ROW_TAG + 10, column);
                ttprintf("Modified data width");
        }
}

CMD_DECL_PRIVATE(brapp, "Open GUI application for this card", pbuf)
{
        struct browser_t *pbr = BROWSER(pbuf);
        gbl.g_isgui = 1;
        vmebr_start_application(pbr->br_pregs, pbr->br_mdata);
}

CMD_DECL_PRIVATE(brchmod,
        "Toggle the default register data width between two byutes and "
        "four bytes.",
        pbuf)
{
        struct browser_t *pbr;

        pbr = BROWSER(pbuf);
        if (pbr->br_dwidth == 2) {
                pbr->br_dwidth   = 4;
                pbr->br_printlen = 10;
                pbr->br_offs     /= 2;
                pbr->br_sel      /= 2;
        } else {
                pbr->br_dwidth   = 2;
                pbr->br_printlen = 6;
                pbr->br_offs     *= 2;
                pbr->br_sel      *= 2;
        }

        /*
         * Compensate for discrepancies in RNM register
         * offsets and browser register offsets
         */
        if (pbr->br_mdata == NULL)
                pbr->br_dwshift = 0;
        else if (pbr->br_dwidth > pbr->br_mdata->r_dwidth)
                pbr->br_dwshift = -1;
        else if (pbr->br_dwidth < pbr->br_mdata->r_dwidth)
                pbr->br_dwshift = 1;
        else /* `==' */
                pbr->br_dwshift = 0;

        pbr->br_ncols = 16 / pbr->br_dwidth;
        brcheckpos(pbr);
}

CMD_DECL_PRIVATE(brchrefresh,
        "Change the screen refresh rate. Parameter is in milliseconds, "
        "or a negative number to turn refresh off. (If refresh is "
        "turned off, the screen will only refresh on each key "
        "input.) ",
        pbuf)
{
        struct browser_t *pbr = BROWSER(pbuf);
        int r;
        int v;
        char *s;

        if (iskeyboard()) {
                ttto(-1);
                r = mbgetval("Enter number of milliseconds: ", &v);
                if (r)
                        goto done;
        } else {
                if ((s = token_next()) == NULL)
                        return;

                v = strtol(s, NULL, 0);
        }

        if (v > 0 && v < 10)
                v = REFRESH_MIN_MS;
        else if (v < 0)
                v = -1;

        pbr->br_refresh = v;
done:
        pbuf->b_mode->m_setto(pbuf);
}

CMD_DECL_PRIVATE(brrefreshoff,
        "Turn off refresh. You may turn on refresh with the command "
        "`refresh-on'.",
        pbuf)
{
        struct browser_t *pbr = BROWSER(pbuf);

        pbr->br_refresh = -1;
        pbuf->b_mode->m_setto(pbuf);
}

CMD_DECL_PRIVATE(braddrnm,
        "Add an RNM file's metadata for this card. If an RNM file is "
        "already attached to this card, that link will be overwritten. "
        "The old RNM will remain in cache in case it is used again. "
        "!!!WARNING!!! Trying to load an updated version of an already "
        "cached RNM (ie one with the same path) will result in the old "
        "cache being used, and the new file will not get loaded. "
        "A command will soon be available to refresh an RNM with its "
        "updated file. It will be called `rnm-refresh'.",
        pbuf)
{
        char *s;
        struct rnm_t *r;
        struct browser_t *pbr = BROWSER(pbuf);

        if (iskeyboard()) {
                if (pbuf->b_rnms != NULL || pbr->br_mdata != NULL)
                        mbwarn("Overwriting old RNM");

                ttto(-1);
                s = mbgets("Enter name of RNM file: ");
                pbuf->b_mode->m_setto(pbuf);
        } else {
                s = token_next();
        }

        if (s == NULL)
                return;

        r = rnmparse(s);
        pbuf->b_rnms = r;
        pbr->br_mdata = r;

        if (pbr->br_strbar != NULL)
                free(pbr->br_strbar);

        pbr->br_strbar = titlemake(dirstrip(s),
                                   (int)pbr->br_region->vme_addr,
                                   pbr->br_crateid);

        /* Force dwshift coherence */
        brchmod(pbuf);
        brchmod(pbuf);
        if (iskeyboard())
                free(s);
}

static void brsetto(struct buffer_t *pbuf)
{
        struct browser_t *pbr = BROWSER(pbuf);
        ttto(pbr->br_refresh);
}

static void brquit(struct buffer_t *pbuf)
{
        struct browser_t *br = BROWSER(pbuf);

        if (br->br_strbar != NULL)
                free(br->br_strbar);

        /*
         * If dummy (and not mapped VME), free all that
         * scratchpad memory
         */
        if (br->br_crateid < 0)
                free(br->br_region->base);

        free(br);
        bufferkill(pbuf);
}

static void brkill(struct buffer_t *pbuf)
{
        struct browser_t *br;
        int vaddr;

        br = BROWSER(pbuf);
        if ((vaddr = (int)br->br_region->vme_addr) < 0) {
                mbwarn("Currently cannot kill CONFIG browsers");
                return;
        }

        brquit(pbuf);
}

CMD_DECL_PRIVATE(brzero,
        "Clear the currently selected register",
        pbuf)
{
        brwrite1(BROWSER(pbuf), 0);
}

static const struct mode_t brmode = {
        .m_name    = "browse",
        .m_left    = brleft,
        .m_right   = brright,
        .m_up      = brup,
        .m_dn      = brdn,
        .m_pgup    = brpgup,
        .m_pgdn    = brpgdn,
        .m_end     = brend,
        .m_home    = brhome,
        .m_splash  = brsplash,
        .m_setto   = brsetto,
        .m_title   = brtitle,
        .m_kill    = brkill,
        .m_quit    = brquit,
};

static struct string_lookup brslu[] = {
        SLU_PARAM("app",         brapp       ),
        SLU_PARAM("ditto",       brditto     ),
        SLU_PARAM("zero",        brzero      ),
        SLU_PARAM("goto",        brgoto      ),
        SLU_PARAM("goto-name",   brgotoname  ),
        SLU_PARAM("write",       brwrite     ),
        SLU_PARAM("chmod",       brchmod     ),
        SLU_PARAM("add-rnm",     braddrnm    ),
        SLU_PARAM("refresh-off", brrefreshoff),
        SLU_PARAM("refresh-on",  brchrefresh ),
        SLU_PARAM("write-reg",   brwrite2name),
        SLU_END,
};

static struct key_lookup brklu[] = {
        KLU_PARAM('d', brditto   ),
        KLU_PARAM('D', brditto   ),
        KLU_PARAM('g', brgoto    ),
        KLU_PARAM('G', brgotoname),
        KLU_PARAM('/', brgoto    ),/* Just like ed! */
        KLU_PARAM('z', brzero    ),
        KLU_PARAM('Z', brzero    ),
        KLU_PARAM('w', brwrite   ),
        KLU_PARAM('W', brwrite   ),
        KLU_PARAM('c', brchmod   ),
        KLU_PARAM('C', brchmod   ),
        KLU_END,
};

/**
 * @brief Allocate a browser for a new VME card.
 *
 * @param name Something that you want to name the new buffer with.
 *
 * @param region An already-initialized, mapped, etc., VME_REGION. See
 * V120.h for more info. The VME region should be unique when making
 * multiple calls to this function. If you are using a dummy region
 * (eg a scratchpad, or something for debugging) you must set it up
 * yourself. If it is a dummy, the vme_addr field should be zero. If
 * it is a V120_CONFIG register block, the vme_addr field should be
 * negative..
 *
 * @param metadata Pointer to RNM byte code, or NULL if you are not
 * using an RNM file for this VME_REGION
 *
 * @param crateid 0 to 15, or a negative integer if dummy
 *
 * @return Pointer to the buffer related to this region, or NULL if
 * a buffer could not be allocated.
 *
 * @warning Do not delete \c region yourself until you have killed
 * the buffer using it!
 */
struct buffer_t *browsernew(char *name, VME_REGION *region,
                            struct rnm_t *metadata, int crateid)
{
        struct browser_t *pbr;
        struct buffer_t  *pbuf;
        char *mdname;
        char *ps;

        if ((pbr = malloc(sizeof(*pbr))) == NULL) {
                mbwarn("Could not allocate new browser");
                goto err_pbr;
        }

        if (metadata != NULL)
                mdname = metadata->r_name;
        else
                mdname = "(no name)";

        /* XXX: cast from unsigned to signed. */
        ps = titlemake(mdname, (int)region->vme_addr, crateid);
        if (ps == NULL)
                goto err_ps;

        pbuf = buffernew(name, &brmode, brslu, brklu, metadata, (void *)pbr);
        if (pbuf == NULL)
                goto err_pbuf;

        pbr->br_sel     = 0;
        pbr->br_offs    = 0;
        pbr->br_region  = region;
        pbr->br_mdata   = metadata;
        pbr->br_strbar  = ps;
        pbr->br_dwshift = 0;
        pbr->br_dwidth  = metadata != NULL ? metadata->r_dwidth : 2;
        pbr->br_lastwrite = 0;
        pbr->br_topoffs = metadata != NULL
                        ? metadata->r_len * metadata->r_dwidth : 128;
        pbr->br_refresh = BROWSER_MS;
        pbr->br_crateid = crateid;
        brchmod(pbuf);
        brchmod(pbuf);
        return pbuf;

err_pbuf:
        free(ps);
err_ps:
        free(pbr);
err_pbr:
        return NULL;
}
