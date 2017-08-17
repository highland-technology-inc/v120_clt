/**
 * @file
 * @brief Code that handles typed (as oposed to file-parsed) inputs.
 *
 * This also has code for the global, canonical key bindings and
 * commands.
 *
 * @author Paul Bailey
 * @date
 */
#include "struct.h"
#include <string.h>
#include <stdlib.h>

STATIC_CMD(inpfileload);

/**
 * @brief Look up a single-keystroke command
 * @param b Pointer to the buffer to act on
 * @param lut Table to look up
 * @param c Key for the command
 *
 * @return Zero if a command was found and executed; non-zero if
 * no command was found.
 */
int
keylookupbytable(struct buffer_t *b, struct key_lookup *lut, int c)
{
        while (lut->lk_key != 0) {
                if (lut->lk_key == c) {
                        lut->lk_fn(b);
                        return 0;
                }
                ++lut;
        }
        return 1;
}

/**
 * @brief Look up a long string command
 * @param b Pointer to the buffer that the command is for
 * @param lut Table to look up
 * @param s long string containing the command
 *
 * @return Zero if a command was found and executed; non-zero if
 * no command was found.
 */
int
stringlookupbytable(struct buffer_t *b, struct string_lookup *lut,
                    const char *s)
{
        while (lut->ls_name != NULL) {
                if (!strcmp(lut->ls_name, s)) {
                        lut->ls_fn(b);
                        return 0;
                }
                ++lut;
        }
        return 1;
}

CMD_DECL_PRIVATE(inpkill,
        "Kill the current buffer. Not that if this is a VME block, "
        "only the buffer will be freed. The VME regions cannot be "
        "unmapped until it is time to kill the entire VME crate.",
        pbuf)
{
        gbl.g_buffer->b_mode->m_kill(gbl.g_buffer);
}

CMD_DECL_PRIVATE(inpquit, "Quit program", pbuf)
{
        if (!iskeyboard()) {
                mbwarn("You cannot exit this program from a script");
                return;
        }
        /* TODO: Obviously, all the cleaning up */
        quit();
}

CMD_DECL_PRIVATE(inpnext, "Go to next display", pbuf)
{
        buffernext();
}

CMD_DECL_PRIVATE(inptoggle,
        "Go to previous display; current display will become "
        "the new `previous' display",
        pbuf)
{
        bufferback();
}

CMD_DECL_PRIVATE(inphome, "Switch display to `buffer list'", pbuf)
{
        buffergoto(gbl.g_headbuf);
}

CMD_DECL_PRIVATE(inpup,
        "Scroll up or move currsor up. Depends on type of display",
        pbuf)
{
        cursup();
}

CMD_DECL_PRIVATE(inpdn,
        "Scroll down or move currsor down. Depends on type of display",
        pbuf)
{
        cursdn();
}

CMD_DECL_PRIVATE(inpleft,
        "Move cursor left, if applicable. Depends on type of display",
        pbuf)
{
        cursleft();
}

CMD_DECL_PRIVATE(inpright,
        "Move cursor right, if applicable. Depends on type of display",
        pbuf)
{
        cursright();
}

CMD_DECL_PRIVATE(inppgup,
        "Move cursor up a page, if applicable. Depends on type of "
        "display",
        pbuf)
{
        curspgup();
}

CMD_DECL_PRIVATE(inppgdn,
        "Move cursor down a page, if applicable. Depends on type of "
        "display",
        pbuf)
{
        curspgdn();
}

CMD_DECL_PRIVATE(inpaddvme,
        "Add a VME card. Parameters are crate number, vme address, and "
        "(optionally) RNM file to associate wit the card.",
        pbuf)
{
        int res, crateid, vmeaddr, vmelen;
        char *rnmfile, *s;

        if (iskeyboard()) {
                res = mbgetval("Enter crate number: ", &crateid);
                if (res)
                        return;

                res = mbgetval("Enter VME address: ", &vmeaddr);
                if (res)
                        return;

                rnmfile = mbgets("Enter name of RNM file: ");
                if (rnmfile == NULL) {
                        return;
                } else if (*rnmfile == '\0') {
                        free(rnmfile);
                        rnmfile = NULL;
                }
        } else {
                if ((s = token_next()) == NULL) {
                        mbwarn("Expected: crate id");
                        return;
                }
                crateid = strtol(s, NULL, 0);
                if ((s = token_next()) == NULL) {
                        mbwarn("Expected: VME address");
                        return;
                }
                vmeaddr = strtoul(s, NULL, 0);
                rnmfile = token_next();
        }

        if (crateid > 15 || crateid < 0) {
                mbwarn("Invalid parameter");
                return;
        }

        vmelen = 65536; /* For now */
        add_vme(crateid, vmeaddr, vmelen, rnmfile);

        if (rnmfile != NULL && iskeyboard())
                free(rnmfile);
}


CMD_DECL_PRIVATE(inpfileload,
        "Load a file running simple commands. The file is parsed by "
        "line; a line is in the form of `command-name:arg0:arg1:...:argn'. "
        "All of a line following a `#' character is ignored and can be "
        "used for comments. Blank lines and leading white space are okay.",
        pbuf)
{
        char *fname;

        if (iskeyboard()) {
                if ((fname = mbgets("Enter name of file: ")) == NULL)
                        return;
        } else {
                if ((fname = token_next()) == NULL)
                        return;
        }

        if (load_file(fname, pbuf))
                mbwarn("Failed to load %s", fname);

        free(fname);
}

CMD_DECL_PRIVATE(inpadddummy,
        "Add a dummy VME card. You don't need it. It's handy for "
        "debugging this program when no V120 or driver is available.",
        pbuf)
{
        add_dummy();
}

static struct string_lookup inpslu[] = {
        SLU_PARAM("home",          inphome),
        SLU_PARAM("quit",          inpquit),
        SLU_PARAM("q",             inpquit),
        SLU_PARAM("Q",             inpquit),
        SLU_PARAM("next-buffer",   inpnext),
        SLU_PARAM("toggle-buffer", inptoggle),
        SLU_PARAM("add-vme",       inpaddvme),
        SLU_PARAM("add-dummy",     inpadddummy),
        SLU_PARAM("kill",          inpkill),
        SLU_PARAM("load-file",     inpfileload),
        SLU_END,
};

#if 0
/* TODO: These */
CMD_DECL_PRIVATE(inprnmrefr,
        "Refresh RNM byte code associated with current card. "
        "WARNING: This will take place for all cards using the same "
        "RNM file (ie with the same absolute path, unless the search "
        "path has been modified (currently not possible). This is "
        "because, in order to conserve memory, RNM metadata is not "
        "duplicated for every browser that uses the same file in the "
        "same path.",
        pbuf)
{
        /* Actually, this should be in browser.c */
}

CMD_DECL_PRIVATE(inprnmrefrall,
        "Refresh all cached RNM byte code with the file paths "
        "associated with them. If the file was moved, deleted, or "
        "otherwise made unavailable, the appurtenant RNM byte code will "
        "not be refreshed, and it will no longer be available after you "
        "close the program.",
        pbuf)
{
}
#endif

CMD_DECL(inpcommand, "Insert a long-string command", pbuf)
{
        char *s;

        if (iskeyboard()) {
                ttto(-1);
                s = mbgets("Enter command: ");
        } else {
                s = token_next();
        }

        if (s == NULL) {
                /*
                 * Not necessarily an error. When parsing from a file,
                 * one NULL command will be passed here.
                 */
                goto done;
        }

        if (stringlookupbytable(gbl.g_buffer, inpslu, s) != 0
            && stringlookup(gbl.g_buffer, s) != 0) {
                        mbwarn("Unknown command");
        }

        if (iskeyboard())
                free(s);

done:
        pbuf->b_mode->m_setto(pbuf);
}

static struct key_lookup inpklu[] = {
        KLU_PARAM('j',       inpdn),
        KLU_PARAM('J',       inpdn),
        KLU_PARAM(KEY_DOWN,  inpdn),
        KLU_PARAM('k',       inpup),
        KLU_PARAM('K',       inpup),
        KLU_PARAM(KEY_UP,    inpup),
        KLU_PARAM('l',       inpright),
        KLU_PARAM('L',       inpright),
        KLU_PARAM(KEY_RIGHT, inpright),
        KLU_PARAM('h',       inpleft),
        KLU_PARAM('H',       inpleft),
        KLU_PARAM(KEY_LEFT,  inpleft),
        KLU_PARAM(TO_CTRL('B'), inppgup),
        KLU_PARAM(KEY_PPAGE, inppgup),
        KLU_PARAM(TO_CTRL('F'), inppgdn),
        KLU_PARAM(KEY_NPAGE, inppgdn),
        KLU_PARAM(':',       inpcommand),
        KLU_PARAM('0',       inphome),
        KLU_PARAM('N',       inpnext),
        KLU_PARAM('n',       inpnext),
        KLU_PARAM('b',       inptoggle),
        KLU_PARAM('B',       inptoggle),
        KLU_END
};

/**
 * @brief Take action on an input
 * @param c Character from user
 *
 * This looks up the canonical commands, then the current
 * buffer's specific commands.
 */
void processinput(int c)
{
        if (c == EOF)
                return;

        if (keylookupbytable(gbl.g_buffer, inpklu, c))
                keylookup(gbl.g_buffer, c);
}
