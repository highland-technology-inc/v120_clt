bin_PROGRAMS = v120
v120_SOURCES = \
  main.c \
  common.c \
  flash.c \
  report.c \
  requester.c \
  reset.c \
  rw.c \
  help.c \
  scan.c \
  v120_common.h

v120_CPPFLAGS = -I$(top_srcdir)/include -Wall
v120_LDADD = $(top_srcdir)/libV120/libV120.la
check_SCRIPTS = v120_test.sh
TESTS = $(check_SCRIPTS)

v120_test.sh:
	echo './v120 -? | grep "v120"' > v120_test.sh
	chmod +x v120_test.sh

CLEANFILES = v120_test.sh
