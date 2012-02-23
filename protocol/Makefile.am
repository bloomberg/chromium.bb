if HAVE_XSLTPROC
doc_DATA = wayland.html wayland-protocol.css

wayland.html: wayland.xml protocol.xsl
	$(AM_V_GEN)$(XSLTPROC) protocol.xsl wayland.xml > $@

endif
