AM_CPPFLAGS = @VMOD_INCLUDES@

vmoddir = @VMOD_DIR@
vmod_LTLIBRARIES = libvmod_msgsend.la

libvmod_msgsend_la_LDFLAGS = -module -export-dynamic -avoid-version -shared

libvmod_msgsend_la_SOURCES = \
	vcc_if.c \
	vcc_if.h \
	vmod_msgsend.c

vcc_if.c vcc_if.h: @VMODTOOL@ $(top_srcdir)/src/vmod_msgsend.vcc
	@VMODTOOL@ $(top_srcdir)/src/vmod_msgsend.vcc

VMOD_TESTS = tests/*.vtc
.PHONY: $(VMOD_TESTS)

tests/*.vtc:
	@VARNISHTEST@ -Dvarnishd=@VARNISHD@ -Dvmod_topbuild=$(abs_top_builddir) $@

check: $(VMOD_TESTS)

EXTRA_DIST = \
	vmod_msgsend.vcc \
	$(VMOD_TESTS)

CLEANFILES = $(builddir)/vcc_if.c $(builddir)/vcc_if.h
