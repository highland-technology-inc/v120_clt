
# FIXME: This is HORRIBLE!! We need some better way than
# to require kernel source tree to be available!
KBUILD_SRC ?= ./

docproc = ../tools/docproc
outdir = v120_irq

vnm := v120_linux_reference
pdfout := $(vnm).pdf
roffin := \
 ../man/v120.1 \
 ../man/v120_tui.1 \
 ../man/v120irqd.8 \
 ../man/v120.4 \
 ../man/v120.7 \
 ../man/V120_PD.3 \
 ../man/v120_open.3 \
 ../man/v120_allocate_vme.3 \
 ../man/v120_dma_status.3 \
 ../man/v120_dma_xfr.3 \
 ../man/v120irqd.7 \
 ../man/v120irqd_ack.3 \
 ../man/v120irqd_client.3 \
 ../man/v120irqd_status.3


.PHONY: all docclean
all: $(docproc) $(pdfout)
	bash docmaker.sh $(KBUILD_SRC)

$(pdfout): $(roffin)
	cat $^ | groff -mman | ps2pdf - > $@

$(docproc): $(docproc).c
	$(MAKE) -C ../tools docproc

cleanfiles := $(outdir) $(docproc) $(pdfout)
docclean:
	$(if $(wildcard $(cleanfiles)),$(RM) -r $(cleanfiles))
