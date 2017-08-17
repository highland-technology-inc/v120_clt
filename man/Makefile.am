v120_man1 = v120.1

v120_man4 = v120.4

v120_man8 = v120irqd.8

v120_man3 = \
 v120_next.3 \
 v120_open.3 \
 v120_close.3 \
 v120_crate.3 \
 v120.3 \
 v120_allocate_vme.3 \
 v120_dma_status.3 \
 v120_dma_xfr.3 \
 v120_add_vme_region.3 \
 v120_delete_vme_list.3 \
 v120_get_vme_region.3 \
 V120_PD.3 \
 v120irqd_ack.3 \
 v120irqd_nak.3 \
 v120irqd_status.3 \
 v120irqd_client.3 \
 v120irqd_getinterrupt.3 \
 v120irqd_interrupt.3 \
 v120irqd_release.3 \
 v120irqd_request.3

v120_man7 = \
 v120.7 \
 v120irqd.7

man1_MANS = $(v120_man1)
if BUILD_V120_TUI
 man1_MANS += v120_tui.1
endif
man3_MANS = $(v120_man3)
man4_MANS = $(v120_man4)
man7_MANS = $(v120_man7)
man8_MANS = $(v120_man8)

EXTRA_DIST = \
 $(v120_man1) \
 $(v120_man3) \
 $(v120_man4) \
 $(v120_man7) \
 $(v120_man8) \
 v120_tui.1

CLEANFILES = \
 v120.3 \
 v120_close.3 \
 v120_crate.3 \
 v120_next.3 \
 v120_get_vme_region.3 \
 v120_add_vme_region.3 \
 v120_delete_vme_list.3 \
 v120irqd_nak.3 \
 v120irqd_release.3 \
 v120irqd_request.3 \
 v120irqd_getinterrupt.3

v120irqd_nak.3: v120irqd_ack.3
	echo ".so man3/$^" > $@

v120.3: v120.7
	echo ".so man7/$^" > $@

v120_close.3 v120_next.3 v120_crate.3: v120_open.3
	echo ".so man3/$^" > $@

v120irqd_getinterrupt.3 v120irqd_release.3 v120irqd_request.3: v120irqd_interrupt.3
	echo ".so man3/$^" > $@

v120_get_vme_region.3 v120_add_vme_region.3 v120_delete_vme_list.3: v120_allocate_vme.3
	echo ".so man3/$^" > $@
