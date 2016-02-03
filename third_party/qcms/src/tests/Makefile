CC=gcc
INCLUDE=-I../
WALL=-Wall
CFLAGS=-O2 -msse2 $(WALL) -DSSE2_ENABLE
LDFLAGS=-lm

QCMS=../transform.c ../transform-sse2.c ../transform_util.c ../matrix.c ../iccread.c ../chain.c
OBJS=$(QCMS:.c=.o)

all: qcms_tests

qcms_tests: qcms_test_main.c qcms_test_munsell.c qcms_test_tetra_clut_rgba.c qcms_test_internal_srgb.c $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDE) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf qcms_tests *.o $(OBJS)
