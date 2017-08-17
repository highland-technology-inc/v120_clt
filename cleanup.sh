#!/bin/sh

# Use this to get rid of clutter you don't want in version control.
# autoreconf and configure are required after executing this script.
files='stamp-h1
config.h
configure
config.log
config.status
libtool
autom4te.cache/
aclocal.m4'

subdirs='test
libV120
libV120irqd
v120irqd
include
examples
man
v120
v120_tui
v120_tui/rc
'

test -f Makefile && make clean
make -C doc docclean

for i in $files; do
    if test -d $i; then
        rm -r $i
    elif test -f $i; then
        rm $i
    fi
done

rm -f build/ltmain.sh
rm -f build/test-driver
rm -f m4/*.m4
rm -rf test/unity/.deps/

for dir in . $subdirs; do
    rm -f ${dir}/*.in
    rm -f ${dir}/Makefile
    rm -rf ${dir}/.deps
    rm -f ${dir}/*~
done
