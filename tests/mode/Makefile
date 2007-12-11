
all: modetest

#CFLAGS = -g -ansi -pedantic -DPOSIX_C_SOURCE=199309L \
#        -D_POSIX_SOURCE -D_XOPEN_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE \

modetest: modetest.c
	@gcc $(CFLAGS) -o modetest -Wall -I../../libdrm -I../../shared-core -L../../libdrm/.libs -ldrm modetest.c 

clean:
	@rm -f modetest
