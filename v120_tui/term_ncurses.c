/*
 * term_ncurses.c - Terminal-specific (well, not really) output code.
 *
 * This is actually a wrapper for the ncurses library. I don't know why
 * ncurses decided to clutter up the namespace with almost-standard-
 * sounding short, patternless function names like 'getch' and 'timeout'
 * and nl (!!). This file is meant to ease the programmer and compiler
 * by keeping the namespace contrained to this .c file (TODO: we'll have
 * to edit struct.h first).
 *
 * If ncurses is not on your system, then termios.c will compile instead,
 * in hopes that your terminal will understand commonplace escape
 * sequences.
 */
#include "struct.h"
#if HAVE_LIBNCURSES
#include <stdarg.h>

volatile int ttnrows_ = 24;
volatile int ttncols_ = 80;

#define MSWAIT_MIN           150

void ttstart(void)
{
        initscr();
        nl();
        timeout(-1);
        ttsize();
        noecho();
        keypad(stdscr, TRUE);
        curs_set(0);
}

void ttexit(void)
{
        endwin();
}

/* Always for the whole screen */
void ttsetfg(int color)
{
        printf("\033[3%dm", color & 0x7U);
}

void ttsetbg(int color)
{
        printf("\033[4%dm", color & 0x7U);
}

void ttsize(void)
{
        ttnrows_ = LINES;
        ttncols_ = COLS;
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
        timeout(ms);
}

void ttcursset(int set)
{
        curs_set(set);
}

void ttmove(int row, int col)
{
        move(row, col);
}

void ttbold(int isbold)
{
        if (isbold)
                attron(A_BOLD);
        else
                attroff(A_BOLD);
}

void ttplain(void)
{
#if 0
        attrset(A_NORMAL);
        ttrev(0);
        ttbold(0);
#else
        attroff(A_ATTRIBUTES);
#endif
}

void tteeop(void)
{
        clear();
}

void ttvprintf(const char *fmt, va_list ap)
{
        vwprintw(stdscr, fmt, ap);
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
        if (isrev)
                attron(A_REVERSE);
        else
                attroff(A_REVERSE);
}

void ttflush(void)
{
        refresh();
}

void ttclose(void)
{
        /*
         * Dummy function. This may support multiple terminals
         * someday
         */
}

int ttgetc(void)
{
        return getch();
}

int ttputc(int c)
{
        return addch(c & 0xFFU);
}

#endif /* HAVE_LIBNCURSES */
