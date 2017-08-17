#!/bin/bash
#
# Note: Must define KBUILD_SRC as the root of a kernel source tree to run.

if [ $# -ne 0 ]
    then KBUILD_SRC=$1
else
    KBUILD_SRC=~/src/linux-4.4.0
fi

thisdir=`pwd`
export SRCTREE=${thisdir}/..
export KBUILD_SRC

echo Using kernel source tree $KBUILD_SRC
# DOCPROC=$KBUILD_SRC/scripts/docproc
DOCPROC=../tools/docproc

DOCSRC=${thisdir}
DOCOBJ=${thisdir}/v120_irq

mkdir -p $DOCOBJ
asciidoc --backend docbook -d book -f $DOCSRC/asciidoc.conf \
         -o $DOCOBJ/v120irqd.xml $DOCSRC/v120irqd.txt
$DOCPROC doc $DOCSRC/v120irqd-api.tmpl \
  | xsltproc --stringparam title "V120 Linux Programmer's Manual" \
             --stringparam shorttitle V120 --stringparam manvolnum 3 \
             $DOCSRC/chapterfix.xsl - > $DOCOBJ/v120irqd-api.xml

XMLTO="xmlto --skip-validation"
$XMLTO xhtml -o $DOCOBJ/html $DOCSRC/v120.xml
$XMLTO man -o $DOCOBJ/man $DOCSRC/v120.xml
$XMLTO pdf --with-dblatex -o $DOCOBJ/pdf $DOCSRC/v120.xml
