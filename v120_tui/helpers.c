/**
 * @file
 * @brief Helpers -- mainly string and pathname helpers
 * @author Paul Bailey
 * @date
 */
#include "struct.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ctype.h>

/*
 * @brief Turn a path with '~' in front into a full path name from the
 * root.
 *
 * @param path Name of path. This must either be a full path name or a
 * path whose name begins with '~/'.
 *
 * @return Name of full path. If the parameter was invalid, or a new
 * string could not be allocated, the return value will be NULL.
 */
static char *home2root(const char *path)
{
        char *ps;
        const char *s;
        char buf[128];

        if (path[0] == '~') {
                if (path[1] != '/')
                        return NULL;
                path += 2;
                snprintf(buf, 128, "%s/%s", gbl.g_home, path);
                buf[127] = '\0';
                s = (const char *)buf;
        } else if (path[0] == '/') {
                s = path;
        } else {
                return NULL;
        }

        ps = malloc(strlen(s));
        if (ps != NULL)
                strcpy(ps, s);

        return ps;
}

/*
 * @brief Get the full path name of a file if it is in the global search
 * path.
 *
 * @param filenmae Name of file that does not begin with '~', '.', or '/'
 *
 * @return Full path of file, or NULL. If not NULL, this should be freed
 * with a call to free().
 *
 * @note This only checks for files, not directories
 */
static char *unk2root(const char *filename)
{
        char buf[512];
        struct pathlist_t *p = gbl.g_path;
        int found;
        char *ret = NULL;

        /* Try this dir */
        strncpy(buf, filename, sizeof(buf));
        found = access(buf, F_OK);
        while (found != 0 && p != NULL) {
                snprintf(buf, sizeof(buf), "%s/%s", p->p_path, filename);
                found = access(buf, F_OK);
                p = p->p_next;
        }

        if (found == 0) {
                ret = malloc(strlen(buf) + 1);
                if (ret != NULL)
                        strcpy(ret, buf);
        } else {
                ret = NULL;
        }
        return ret;
}

/**
 * @brief Cribbed and C-ized from a BASIC version of this found in
 * HTI's VREADER program
 */
int editline(char *s, int n, int row, int col, int *cpos, const char *exit)
{
        int tpos;
        int tail = strlen(s);
        int ret = 033;
        int c, i;

        tpos = *cpos;
        if (tpos > tail)
                tpos = tail;
        else if (tpos < 0)
                tpos = 0;

        ttcursset(1);
        while (1) {
                spaceprint(row, col, n);
                ttmove(row, col);
                ttprintf("%s", s);
                ttmove(row, col + tpos);
                ttflush();

                c = ttgetc();
                const char *p = exit;
                while (*p != '\0') {
                        if (c == *p) {
                                ret = c;
                                goto done;
                        }
                        ++p;
                }

                switch (c) {
                case 0177:
                case KEY_BACKSPACE:
                case TO_CTRL('H'): /* backspace */
                        if (tpos > 0) {
                                --tpos;
                                --tail;
                                for (i = tpos; i < tail; ++i)
                                        s[i] = s[i + 1];
                        }
                        break;

                case KEY_DC:
                        if (tpos < tail) {
                                for (i = tpos; i < tail; ++i)
                                        s[i] = s[i + 1];
                                --tail;
                        }
                        break;

                case KEY_LEFT:
                        if (tpos > 0)
                                --tpos;
                        break;

                case KEY_RIGHT:
                        if (tpos < tail)
                                ++tpos;
                        break;

                case '\n':
                        ret = c;
                        goto done;

                case 033:
                        ret = c;
                        goto done;

                default:
                        if (!isprint(c))
                                break;

                        if (tail >= (n - 1)) {
                                mbwarn("Too many characters");
                                break;
                        }

                        for (i = tail; i >= tpos; --i)
                                s[i] = s[i - 1];

                        s[tpos] = c;
                        ++tpos;
                        ++tail;
                        break;
                }
                s[tail] = '\0';
        }
done:
        *cpos = tpos;
        spaceprint(row, col, n);
        ttmove(row, col);
        ttprintf("%s", s);
        ttmove(row, col + tpos);
        ttcursset(0);
        return ret;
}

/**
 * @brief Print a number of spaces
 * @param row row of line
 * @param col start column
 * @param len Number of spaces to print
 */
void spaceprint(int row, int col, int len)
{
        ttmove(row, col);
        while (len > 0) {
                ttputc(' ');
                --len;
        }
}

/**
 * @brief Highlight print a line.
 * @param s String to print. The character '^' starts a highlight, and
 * the character '`' ends a highlight.
 */
void highlight(const char *s)
{
        int c;
        while ((c = *s) != '\0') {
                ++s;
                if (c == '^')
                        ttbold(1);
                else if (c == '`')
                        ttbold(0);
                else
                        ttputc(c);
        }
        ttbold(0);
}


/**
 * @brief Get the full path of a file that is either in the global search
 * path or from a known prefix.
 * @param filename Name of file that can begin with '~', '/', '.', or a
 * file name. If the prefix is not given (IE just a file name), the
 * global path will be searched instead.
 * @return Full path name, or NULL
 * @note This only works for files, not directories, and it only checks
 * for read permissions, not write permissions.
 */
char *getfullpath(const char *filename)
{
        char *pathnew;
        switch (filename[0]) {
        case '~':
        case '/':
                pathnew = home2root(filename);
                break;
        default:
                /* Starts wilth './', '../', or perhaps a letter */
                pathnew = unk2root(filename);
                break;
        }
        return pathnew;
}

/**
 * @brief Open a file either in the global search path or from a known
 * prefix
 * @param filename Name of file that can begin with ~, /, ., or a file
 * name. If the prefix is not given (IE just a file name), the global
 * path will be searched instead
 * @return File pointer or NULL. If not NULL, remember to call fclose()
 * on the return value when you are finished with it.
 */
FILE *infilesearch(const char *filename)
{
        char *pathnew = getfullpath(filename);
        FILE *fp = NULL;

        if (pathnew != NULL) {
                fp = fopen(pathnew, "r");
                free(pathnew);
        }

        return fp;
}

/**
 * @brief Get a pointer to only the file name, without the path.
 * @return Modified pointer in the same string.
 * @warning This does not modify the contents of the string, but the
 * return value is on the same string. Do not free the return value,
 * and do not modify the contents of the return value unless you know
 * what you are doing.
 */
char *dirstrip(char *name)
{
        char *s = name + strlen(name) - 1;
        while (*s != '/' && s >= name)
                --s;

        return s + 1;
}

/**
 * @brief Calculate hash number for a string
 * @param s String to hash
 * @return hash number
 */
hash_t hashstring(const char *s)
{
        hash_t hash = 0;
        while (*s != 0) {
                hash = 31 * hash + *s;
                ++s;
        }

        if (hash < 0)
                hash = -hash;

        return hash;
}
