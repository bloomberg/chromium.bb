
all: app

#CFLAGS = -g -ansi -pedantic -DPOSIX_C_SOURCE=199309L \
#        -D_POSIX_SOURCE -D_XOPEN_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE \

app: demo.c
	@gcc $(CFLAGS) -o app -Wall demo.c

clean:
	@rm -f app

run: app
	sudo ./test
