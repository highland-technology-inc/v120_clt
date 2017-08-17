/**
 * @file
 * @brief Non-terminal-specific display manager
 *
 * These functions are the common nodes for writing to the
 * different parts of the display (minibuffer, mode line, etc.)
 *
 * @author Paul Bailey
 * @date
 */
#include "struct.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * @brief Print the mode line.
 *
 * We'll likely have more to do here in the future.
 */
void modeline(void)
{
        const char *picohelp = "Type `:q' to quit";
        const char *name = modename();
        char modeln[200];     /* Overkill, I know */
        int count, n;

        count = snprintf(modeln, sizeof(modeln), "- %s -- %s ", name,
                         picohelp);
        n = ttncols() - count;

        ttmove(ttnrows() - 2, 0);
        ttrev(1);
        ttprintf("%s", modeln);
        while (n > 0) {
                ttputc('-');
                --n;
        }
        ttrev(0);
}

/**
 * @brief Print the title of the current buffer on the top of the screen
 */
void titleprint(const char *title)
{
        int ncols = ttncols();
        int len = strlen(title);
        int pad = ncols - len / 2;
        int start = (ncols - len) / 2; /* If you see the difference...  */
        ttmove(0, 0);

        ttrev(1);

        while (start > 0) {
                ttputc(' ');
                --start;
        }
        ttprintf("%s", title);
        while (pad > 0) {
                ttputc(' ');
                --pad;
        }

        ttrev(0);
}

/* Reposition cursor to start of minibuffer */
static void brepos(int col)
{
        ttmove(ttnrows() - 1, col);
}

/**
 * @brief Prompt user for a string (command or something)
 * @param prompt String to prompt the user with.
 * @return User's string, or NULL if the user escaped or the string
 * could not be allocated. If non-NULL, the string should be freed
 * by a call to free() when it is no longer needed.
 */
char *mbgets(const char *prompt)
{
        register int outchar;
        int startcol;
        int len;
        int row;
        int ncols;
        int cpos = 0;
        char s[128];
        char *ret = NULL;

        memset(s, 0, sizeof(s));
        while (1) {
                /* Do this for every input, in case screen is resized */
                brepos(0);
                ttprintf("%s", prompt);
                startcol = strlen(prompt);
                ncols    = ttncols();
                row      = ttnrows() - 1;
                len      = ncols - startcol - 2;

                outchar = editline(s, len, row, startcol, &cpos, "");

                switch (outchar) {
                case '\n':
                        if ((ret = malloc(strlen(s) + 1)) != NULL)
                                strcpy(ret, s);

                        goto done;
                /* TODO: case '\t': and tab completion */
                case 033:
                default:
                        /* Escape or error */
                        ret = NULL;
                        goto done;
                }
        }
done:
        return ret;
}

/**
 * @brief Get an input number from user, using the minibuffer
 * @param prompt String to prompt the user
 * @param pval Pointer to a variable storing the value
 * @return 0 if pval changed; a negative number if user escaped.
 *
 * This defaults to hexadecimal. User can chage base by pushing
 * 'T' for decimal (which will require '0x' for hexadecimal or
 * 'H' for hexadecimal. Base will display in the right hand side
 * of the minibuffer.
 */
int mbgetval(const char *prompt, int *pval)
{
        register int outchar;
        int startcol, basecol;
        int len;
        int row;
        int ncols;
        int cpos = 0;
        int base = 16;
        char s[128];

        memset(s, 0, sizeof(s));
        while (1) {
                /* Do this for every input, in case screen is resized */
                brepos(0);
                ttprintf("%s", prompt);
                startcol = strlen(prompt);
                ncols    = ttncols();
                row      = ttnrows() - 1;
                len      = ncols - startcol - 2;
                basecol  = ncols - 1;

                brepos(basecol);
                ttputc(base ? 'H' : 'T');

                outchar = editline(s, len, row, startcol, &cpos, "hHtT");
                switch (outchar) {
                case '\n':
                        /* Finished */
                        *pval = strtol(s, NULL, base);
                        return 0;
                case 'h':
                case 'H':
                        base = 16;
                        break;
                case 't':
                case 'T':
                        base = 0;
                        break;
                case 033:
                default:
                        /* Escape or error */
                        return -1;
                }
        }
}

/**
 * @param Clear the line in the minibuffer
 */
void mbclear(void)
{
        brepos(0);
        spaceprint(ttnrows() - 1, 0, ttncols() - 1);
}

void mbwarn(const char *fmt, ...)
{
        va_list ap;

        mbclear();

        brepos(0);

        va_start(ap, fmt);
        ttvprintf(fmt, ap);
        va_end(ap);

        ttflush();
        sleep(1);
}
