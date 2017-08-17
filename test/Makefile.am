AUTOMAKE_OPTIONS=subdir-objects
test_interrupt_structs_SOURCES = \
  test_interrupt_structs.c \
  unity/unity.c
test_interrupt_structs_LDADD = \
  $(top_srcdir)/libV120/libV120.la \
  $(top_srcdir)/libV120irqd/libV120irqd.la
test_interrupt_structs_CPPFLAGS = -I$(top_srcdir)/include

test_irq_vector_table_SOURCES = \
  test_irq_vector_table.c \
  unity/unity.c \
  ../v120irqd/irq_vector_table.c
test_irq_vector_table_LDADD = \
  $(top_srcdir)/libV120/libV120.la \
  $(top_srcdir)/libV120irqd/libV120irqd.la
test_irq_vector_table_CPPFLAGS = -I$(top_srcdir)/include

test_server_SOURCES = \
  test_server.c \
  unity/unity.c
test_server_LDADD = \
  $(top_srcdir)/libV120/libV120.la \
  $(top_srcdir)/libV120irqd/libV120irqd.la
test_server_CPPFLAGS = \
  -I$(top_srcdir)/include \
  -DDAEMON_LOCAL_NAME=\"../v120irqd/v120irqd\"
check_PROGRAMS = test_interrupt_structs test_irq_vector_table test_server
TESTS = $(check_PROGRAMS)
EXTRA_DIST = unity
