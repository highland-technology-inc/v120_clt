bin_PROGRAMS = v120irqd
v120irqd_SOURCES = v120irqd.c irq_vector_table.c
v120irqd_CPPFLAGS = -I$(top_srcdir)/include
v120irqd_LDADD = \
  $(top_srcdir)/libV120/libV120.la \
  $(top_srcdir)/libV120irqd/libV120irqd.la
EXTRA_DIST = 30-v120irqd.conf README.txt
