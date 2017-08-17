local_examples = \
  fakeirq \
  client \
  server_status \
  v120fakeirq \
  v680test \
  v680test2 \
  dmatest \
  dmawrite \
  polltest

if BUILD_EXAMPLES
 noinst_PROGRAMS = $(local_examples)
 fakeirq_SOURCES = fakeirq.c
 fakeirq_CPPFLAGS = -I$(top_srcdir)/include
 fakeirq_LDADD = \
   $(top_srcdir)/libV120/libV120.la \
   $(top_srcdir)/libV120irqd/libV120irqd.la
 client_SOURCES = client.c
 client_CPPFLAGS = -I$(top_srcdir)/include
 client_LDADD = \
   $(top_srcdir)/libV120/libV120.la \
   $(top_srcdir)/libV120irqd/libV120irqd.la
 server_status_SOURCES = server_status.c
 server_status_CPPFLAGS = -I$(top_srcdir)/include
 server_status_LDADD = \
   $(top_srcdir)/libV120/libV120.la \
   $(top_srcdir)/libV120irqd/libV120irqd.la
 v120fakeirq_SOURCES = v120fakeirq.c
 v120fakeirq_CPPFLAGS = -I$(top_srcdir)/include
 v120fakeirq_LDADD = \
   $(top_srcdir)/libV120/libV120.la \
   $(top_srcdir)/libV120irqd/libV120irqd.la
 v680test_SOURCES = v680test.c
 v680test_CPPFLAGS = -I$(top_srcdir)/include
 v680test_LDADD = \
   $(top_srcdir)/libV120/libV120.la \
   $(top_srcdir)/libV120irqd/libV120irqd.la
 v680test2_SOURCES = v680test2.c
 v680test2_CPPFLAGS = -I$(top_srcdir)/include
 v680test2_LDADD = \
   $(top_srcdir)/libV120/libV120.la \
   $(top_srcdir)/libV120irqd/libV120irqd.la
 dmatest_SOURCES = dmatest.c
 dmatest_CPPFLAGS = -I$(top_srcdir)/include
 dmatest_LDADD = \
   $(top_srcdir)/libV120/libV120.la
 dmawrite_SOURCES = dmawrite.c
 dmawrite_CPPFLAGS = -I$(top_srcdir)/include
 dmawrite_LDADD = \
   $(top_srcdir)/libV120/libV120.la
 polltest_SOURCES = polltest.c
else
 EXTRA_DIST = \
   fakeirq.c client.c server_status.c \
   v120fakeirq.c v680test.c v680test2.c \
   dmatest.c dmawrite.c polltest.c
endif
