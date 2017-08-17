SUBDIRS = rc
local_v120_tui_sources = \
  browser.c \
  termios.c \
  buffer.c \
  controlreg.c \
  display.c \
  helpers.c \
  main.c \
  rnm.c \
  struct.h \
  term_ncurses.c \
  input.c \
  quit.c \
  token.c \
  gui.c

if BUILD_V120_TUI
bin_PROGRAMS = v120_tui
appdatadir = $(datadir)/v120_tui
v120_tui_SOURCES = $(local_v120_tui_sources)
v120_tui_CPPFLAGS = -I$(top_srcdir)/include -Wall -std=gnu99 \
	-DRCDATADIR=\"$(appdatadir)\"
v120_tui_LDADD = $(top_srcdir)/libV120/libV120.la
else
EXTRA_DIST = $(local_v120_tui_sources)
endif
