/*
 * termios.c - VT100-ish alternative to term_ncurses.c
 *
 * This uses termios.h functions and VT100 escape sequences to manipulate
 * the terminal display.  This is less optimum, so if ncurses exists,
 * term_ncurses.c will compile instead of this.
 */
#include "struct.h"
#if !HAVE_LIBNCURSES
#include <stdarg.h>

volatile int ttnrows_ = 24;
volatile int ttncols_ = 80;

#define MSWAIT_MIN           150

#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <ctype.h>

static unsigned int mswait = MSWAIT_MIN;
static struct termios orig_termios;
static int have_term = 0;

void term_err(const char *msg)
{
        fprintf(stderr, "Cannot start terminal: %s", msg);
        if (errno) {
                fprintf(stderr, " (%s)", strerror(errno));
        }
        putc('\n', stderr);
        exit(EXIT_FAILURE);
}

void ttstart(void)
{
        struct termios raw;

        if (!isatty(STDIN_FILENO))
                term_err("not a tty");

        if (tcgetattr(STDIN_FILENO, &orig_termios) < 0)
                term_err("tcgetattr() fail");
        have_term = 1;

        memcpy(&raw, &orig_termios, sizeof(struct termios));

        /* FIXME: Find where this disables interrupts and reenable them */
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

        raw.c_cflag |= (CS8);
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

        raw.c_cc[VMIN]  = 5;
        raw.c_cc[VTIME] = 8;
        raw.c_cc[VMIN]  = 0;
        raw.c_cc[VTIME] = 0;
        raw.c_cc[VMIN]  = 2;
        raw.c_cc[VTIME] = 0;
        raw.c_cc[VMIN]  = 0;
        raw.c_cc[VTIME] = 8;

        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0)
                term_err("tcsetattr() fail");
}

void ttexit(void)
{
        /* Bail early if we did not save this */
        if (have_term) {
                if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) < 0)
                        term_err("tcsetattr() fail");
        }
}

static void print_special(int v, int t)
{
        printf("\033[%d%c", v, t);
        fflush(stdout);
}

static void ttcolor(int color, int t)
{
        if (color != COLOR_DEFAULT)
                color &= 7;
        print_special(30 + color, t);
}

void ttsetfg(int color)
{
        ttcolor(color, 'd');
}

void ttsetbg(int color)
{
        ttcolor(color, 'm');
}

void ttsize(void)
{
        int res;
        struct winsize w;

        res = ioctl(0, TIOCGWINSZ, &w);
        if (res != -1) {
                ttnrows_ = w.ws_row;
                ttncols_ = w.ws_col;
        } else {
                /* Just guess the VT100 defaults */
                ttnrows_ = 20;
                ttncols_ = 73;
        }
}

void ttopen(void)
{
        /*
         * Dummy function. This may support multiple terminals
         * someday
         */
}

void ttto(int ms)
{
        if (ms >= 0 && ms < MSWAIT_MIN)
                ms = MSWAIT_MIN;
        mswait = ms;
}

void ttcursset(int set)
{
        if (set)
                printf("\033[?25h");
        else
                printf("\033[?25l");
        fflush(stdout);
}

void ttmove(int row, int col)
{
        printf("\033[%d;%dH", row + 1, col);
        fflush(stdout);
}

void ttbold(int isbold)
{
        print_special(isbold ? 1 : 22, 'm');
}

void ttplain(void)
{
        print_special(0, 'm');
}

void tteeop(void)
{
        ttplain();
        ttmove(0, 0);
        print_special(2, 'J');
}

void ttvprintf(const char *fmt, va_list ap)
{
        vfprintf(stderr, fmt, ap);
}

void ttprintf(const char *fmt, ...)
{
        va_list ap;

        va_start(ap, fmt);
        ttvprintf(fmt, ap);
        va_end(ap);
}

void ttrev(int isrev)
{
        printf("\033[%dm", isrev ? 7 : 27);
        fflush(stdout);
}

void ttflush(void)
{
        fflush(stdout);
}

void ttclose(void)
{
        /*
         * Dummy function. This may support multiple terminals
         * someday
         */
}

static int
stdin_wait(unsigned int us)
{
        fd_set rfds;
        struct timeval tv;

        FD_ZERO(&rfds);
        FD_SET(0, &rfds);
        tv.tv_sec = us / 1000000;
        tv.tv_usec = us % 1000000;

        return select(1, &rfds, NULL, NULL, &tv);
}

static int
process_escape(void)
{
        int c;
        c = getchar();
        switch (c) {
        case 033:
                /* Multiple quick presses of ESC */
                fflush(stdin);
                return c;
        case '[':
                c = getchar();
                switch (c) {
                case 'A':
                        return KEY_UP;
                case 'B':
                        return KEY_DOWN;
                case 'C':
                        return KEY_RIGHT;
                case 'D':
                        return KEY_LEFT;
                case 'H':
                        return KEY_HOME;
                case 'F':
                        return KEY_END;
                case '1':
                case '7':
                        if ((c = getchar()) == '~')
                                return KEY_HOME;
                        break;
                case '3':
                        if ((c = getchar()) == '~')
                                return KEY_DC;
                        break;
                case '4':
                case '8':
                        if ((c = getchar()) == '~')
                                return KEY_END;
                        break;
                case '5':
                        if ((c = getchar()) == '~')
                                return KEY_PGUP;
                        break;
                case '6':
                        if ((c = getchar()) == '~')
                                return KEY_PGDN;
                        break;
                }
                break;
        case '0':
                c = getchar();
                switch (c) {
                case 'A' | 0x80:
                        return KEY_UP;
                case 'B' | 0x80:
                        return KEY_DOWN;
                case 'C' | 0x80:
                        return KEY_RIGHT;
                case 'D' | 0x80:
                        return KEY_LEFT;
                case 'H' | 0x80:
                        return KEY_HOME;
                case 'F' | 0x80:
                        return KEY_END;
                }
                break;
                /* TODO: Other characters and meta */
        default:
#if 0
                /* TODO: 'd be great to support this */
                if (isalpha(c))
                        return c | KEY_META;
#endif
                break;
        }

        /* Discard undefined key sequences */
        fflush(stdin);
        return 033;
}

/* TODO: 'd be great to support this */
#define KEY_ERROR -1
#define KEY_TIMEOUT -1
int ttgetc(void)
{
        int retval;

        fflush(stdin);
        if (mswait < 0) {
                retval = 1;
        } else {
                /* Watch stdin (fd 0) to see when it has input */
                retval = stdin_wait(mswait * 1000);
        }

        /* Don't rely on value of tv now! */
        if (retval == -1) {
                return KEY_ERROR;
        } else if (retval != 0) {
                int c = getchar();
                if (c == '\r')
                        c = '\n';
                else if (c == 033)
                        c = process_escape();
                else if (c == TO_CTRL('H'))
                        c = KEY_BACKSPACE;
                return c;
        }
        return KEY_TIMEOUT;
}

int ttputc(int c)
{
        int ret;
        if (c == '\n')
                putchar('\r');
        ret = putchar(c);
        fflush(stdout);
        return ret;
}


#endif /* !HAVE_LIBNCURSES */
