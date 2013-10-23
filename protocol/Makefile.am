dist_pkgdata_DATA = wayland.xml wayland.dtd

if HAVE_XMLLINT
.PHONY: validate

.%.xml.valid: %.xml
	$(AM_V_GEN)$(XMLLINT) --noout --dtdvalid $(srcdir)/wayland.dtd $^ > $@

validate: .wayland.xml.valid

all-local: validate

CLEANFILES = .wayland.xml.valid
endif
