CC=gcc
INCLUDE=-I../
WALL=-Wall
CFLAGS=-O2 -msse2 $(WALL) -DSSE2_ENABLE
LDFLAGS=-lm

QCMS=../transform.c ../transform-sse2.c ../transform_util.c ../matrix.c ../chain.c
OBJS=$(QCMS:.c=.o)

all: qcms_test

qcms_test: qcms_test_tetra_clut_rgba.c $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDE) $(LDFLAGS) $(OBJS) qcms_test_tetra_clut_rgba.c -o $@

clean:
	rm -rf qcms_test *.o $(OBJS)
