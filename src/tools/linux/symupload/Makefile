CC=g++

CPPFLAGS=-gstabs -I../../.. -Wall -D_REENTRANT

.PHONY:all clean

BIN=minidump_upload sym_upload

all:$(BIN)

DUMP_UPLOAD_OBJ=minidump_upload.o http_upload.o
SYM_UPLOAD_OBJ=sym_upload.o http_upload.o

minidump_upload:$(DUMP_UPLOAD_OBJ)
	$(CC) $(CPPFLAGS) `curl-config --libs` -o $@ $^

sym_upload:$(SYM_UPLOAD_OBJ)
	$(CC) $(CPPFLAGS) `curl-config --libs` -o $@ $^

http_upload.o:../../../common/linux/http_upload.cc
	$(CC) $(CPPFLAGS) `curl-config --cflags` -c $^

clean:
	rm *.o $(BIN)
