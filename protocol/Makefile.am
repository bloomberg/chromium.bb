if HAVE_XSLTPROC
dist_doc_DATA = wayland.html wayland-protocol.css wayland.xml

wayland.html: $(srcdir)/wayland.xml $(srcdir)/protocol.xsl
	$(AM_V_GEN)$(XSLTPROC) $(srcdir)/protocol.xsl $(srcdir)/wayland.xml > $@

CLEANFILES = wayland.html
endif
