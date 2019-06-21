ACLOCAL_AMFLAGS = -I m4
SUBDIRS = include libV120 libV120irqd v120irqd v120 test examples man v120_tui

# Since we're running automake before releasing a distribution package,
# I am perfecly okay with leaving this non-POSIX wildcard in.
EXTRA_DIST = \
 driver/10-v120.rules \
 driver/COPYING \
 driver/dkms.conf \
 driver/Makefile \
 driver/README.rst \
 driver/v120_dma.c \
 driver/v120_driver.c \
 driver/v120_struct.h \
 driver/v120_uapi.h \
 doc/v120_linux_reference.pdf \
 doc/v120_irq/html/ch03.html \
 doc/v120_irq/html/re06.html \
 doc/v120_irq/html/re03.html \
 doc/v120_irq/html/re01.html \
 doc/v120_irq/html/re08.html \
 doc/v120_irq/html/ch01.html \
 doc/v120_irq/html/index.html \
 doc/v120_irq/html/re10.html \
 doc/v120_irq/html/ch02s04.html \
 doc/v120_irq/html/re04.html \
 doc/v120_irq/html/ch02s02.html \
 doc/v120_irq/html/ch02.html \
 doc/v120_irq/html/re11.html \
 doc/v120_irq/html/ch02s03.html \
 doc/v120_irq/html/ch02s05.html \
 doc/v120_irq/html/re05.html \
 doc/v120_irq/html/re09.html \
 doc/v120_irq/html/re02.html \
 doc/v120_irq/html/re07.html \
 doc/v120_irq/pdf/v120.pdf

bashcompletiondir = ${BASH_COMPLETION_DIR}
dist_bashcompletion_DATA = completions/v120

dist-hook:
	rm -rf `find $(distdir) -name .svn`
