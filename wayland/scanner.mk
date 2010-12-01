%-protocol.c : $(top_srcdir)/protocol/%.xml
	$(AM_V_GEN)$(top_builddir)/wayland/scanner code < $< > $@

%-server-protocol.h : $(top_srcdir)/protocol/%.xml
	$(AM_V_GEN)$(top_builddir)/wayland/scanner server-header < $< > $@

%-client-protocol.h : $(top_srcdir)/protocol/%.xml
	$(AM_V_GEN)$(top_builddir)/wayland/scanner client-header < $< > $@
