
# These definitions are for handling dependencies in the out of kernel build.

PROGS = dristat drmstat

CLEANFILES = *.o *.ko $(PROGS) .depend .*.flags .*.d 

# Build test utilities

PRGCFLAGS = $(CFLAGS) -g -ansi -pedantic -DPOSIX_C_SOURCE=199309L \
	    -D_POSIX_SOURCE -D_XOPEN_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE \
	    -I. -I../libdrm -I../shared-core

DRMSTATLIBS = -L../libdrm -ldrm


programs: $(PROGS)

dristat: dristat.c
	$(CC) $(PRGCFLAGS) $< -o $@

drmstat: drmstat.c
	$(CC) $(PRGCFLAGS) $< -o $@ $(DRMSTATLIBS)

clean:
	rm -f $(CLEANFILES)

