include ../config.mk

libs = libwayland-server.so libwayland-client.so

all : $(libs) scanner

headers =					\
	wayland-util.h				\
	wayland-server-protocol.h		\
	wayland-server.h			\
	wayland-client-protocol.h		\
	wayland-client.h \

libwayland-server.so :				\
	wayland-protocol.o			\
	wayland-server.o			\
	event-loop.o				\
	connection.o				\
	wayland-util.o				\
	wayland-hash.o

libwayland-client.so :				\
	wayland-protocol.o			\
	wayland-client.o			\
	connection.o				\
	wayland-util.o				\
	wayland-hash.o

wayland-server.o : wayland-server-protocol.h
wayland-client.o : wayland-client-protocol.h

wayland-protocol.c : protocol.xml scanner
	./scanner code < $< > $@

wayland-server-protocol.h : protocol.xml scanner
	./scanner server-header < $< > $@

wayland-client-protocol.h : protocol.xml scanner
	./scanner client-header < $< > $@

$(libs) : CFLAGS += -fPIC $(FFI_CFLAGS)
$(libs) : LDLIBS += $(FFI_LIBS)
$(libs) :
	gcc -shared $^ $(LDLIBS)  -o $@

scanner :					\
	scanner.o				\
	wayland-util.o

scanner : LDLIBS += $(EXPAT_LIBS)

install : $(libs)
	install -d $(libdir) $(includedir) $(libdir)/pkgconfig
	install wayland-server.pc wayland-client.pc $(libdir)/pkgconfig
	install $(libs) $(libdir)
	install $(headers) $(includedir)

clean :
	rm -f scanner *.o *.so .*.deps
	rm -f wayland-protocol.c \
		wayland-server-protocol.h wayland-client-protocol.h
