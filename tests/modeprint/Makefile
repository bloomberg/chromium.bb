
all: app

#CFLAGS = -g -ansi -pedantic -DPOSIX_C_SOURCE=199309L \
#        -D_POSIX_SOURCE -D_XOPEN_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE \

app: modetest.c
	@gcc $(CFLAGS) -o app -Wall -I../../libdrm -I../../shared-core -L../../libdrm/.libs -ldrm modetest.c

clean:
	@rm -f app

run: app
	@sudo ./test
