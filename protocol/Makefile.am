protocol_sources =				\
	desktop-shell.xml			\
	screenshooter.xml			\
	xserver.xml				\
	text.xml				\
	input-method.xml			\
	workspaces.xml				\
	text-cursor-position.xml		\
	wayland-test.xml			\
	xdg-shell.xml				\
	scaler.xml

if HAVE_XMLLINT
.PHONY: validate

.%.xml.valid: %.xml
	$(AM_V_GEN)$(XMLLINT) --noout --dtdvalid $(dtddir)/wayland.dtd $^ > $@

validate: $(protocol_sources:%.xml=.%.xml.valid)

all-local: validate

CLEANFILES= $(protocol_sources:%.xml=.%.xml.valid)
EXTRA_DIST=$(protocol_sources)

endif
