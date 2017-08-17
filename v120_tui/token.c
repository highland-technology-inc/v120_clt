#include "struct.h"
#include <string.h>
#include <ctype.h>

#define NTOKENS 100 /* Max number of args + 1 command */
#define NARGS   10
#define LINE_MAX        256

struct token_buf {
        char *argv[NARGS];
        int argc;
        int argi;
        char line[LINE_MAX];
};

static struct token_buf tokenstack[NTOKENS];
static struct token_buf *ptok = &tokenstack[0];

static char *tokcslide(char *s, int c)
{
        while (*s != c && *s != '\0') {
                if (*s == '\\' && s[1] != '\0') {
                        ++s;
                }
                ++s;
        }
        return s;
}

/**
 * @brief Push the arguments to the command interpreter for the
 * current function onto the stack.
 *
 * @return 0 normally, and -1 if the stack is full
 */
int tokenpush(void)
{
        if (ptok < &tokenstack[NTOKENS - 1]) {
                ++ptok;
                return 0;
        } else {
                return -1;
        }
}

/* XXX: Return value, for debugging, or to bait fish withall */
/**
 * @brief Pop the arguments to the command interpreter for the
 * parent function off the stack.
 */
void tokenpop(void)
{
        if (ptok > &tokenstack[0])
                --ptok;
}

/**
 * @brief Tokenize the next non-blank line from the current instream.
 *
 * This is for the single-line parser, and will soon be replaced.
 */
void token_start(void)
{
        char *s;

        if (iskeyboard())
                return;

        ptok->argc = 0;
        ptok->argi = 0;
        memset(ptok->argv, 0, sizeof(ptok->argv));

tgetl:
        memset(ptok->line, 0, LINE_MAX);
        s = fgets(ptok->line, LINE_MAX, gbl.g_instream);
        if (s == NULL)
                return;

        s[LINE_MAX - 1] = '\0';

        for (;;) {
                while (isblank(*s))
                        ++s;

                switch (*s) {
                case '\0':
                case '\r':
                case '\n':
                case '#':
                        /* EOL or comment */
                        if (ptok->argc == 0) {
                                /* Empty line; get next line instead */
                                goto tgetl;
                        }
                        return;

                case '"':
                        /* Include quotes in argument */
                        ptok->argv[ptok->argc] = s;
                        ++s;
                        tokcslide(s, '"');
                        ++s;
                        goto nextarg;

                case '\'':
                        /* Do not include quotes in argument */
                        ++s;
                        ptok->argv[ptok->argc] = s;
                        tokcslide(s, '\'');
                        goto nextarg;

                default:
                        /* Regular argument */
                        ptok->argv[ptok->argc] = s;
                        while (!isspace(*s) && *s != '\0')
                                ++s;
                        goto nextarg;
                }

        nextarg:
                ++ptok->argc;
                if (ptok->argc == NARGS || *s == '\0')
                        return;

                *s = '\0';
                ++s;
                /* continue */
        }
}

void token_end(void)
{
        /*
         * Do any free()ing or stuff like that, if it ever gets that
         * complicated.
         */
}

char *token_next(void)
{
        int i = ptok->argi;

        if (i < ptok->argc) {
                ptok->argi = i + 1;
                return (*ptok->argv[i] == '\0') ? NULL : ptok->argv[i];
        } else {
                return  NULL;
        }
}

int token_nargs(void)
{
        return ptok->argc;
}
