if HAVE_XSLTPROC
doc_DATA = wayland.html
dist_doc_DATA = wayland-protocol.css wayland.xml protocol.xsl

wayland.html: wayland.xml protocol.xsl
	$(AM_V_GEN)$(XSLTPROC) $(srcdir)/protocol.xsl $(srcdir)/wayland.xml > $@

CLEANFILES = wayland.html
endif
