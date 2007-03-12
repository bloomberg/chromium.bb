CPP=g++
CC=gcc

CPPFLAGS=-gstabs -I../../.. -Wall -D_REENTRANT
LDFLAGS=-lssl

.PHONY:all clean

BIN=dump_syms

all:$(BIN)

DUMP_OBJ=dump_symbols.o guid_creator.o dump_syms.o file_id.o

dump_syms:$(DUMP_OBJ)
	$(CPP) $(CPPFLAGS) $(LDFLAGS) -o $@ $^

dump_symbols.o:../../../common/linux/dump_symbols.cc
	$(CPP) $(CPPFLAGS) -c $^

guid_creator.o:../../../common/linux/guid_creator.cc
	$(CPP) $(CPPFLAGS) -c $^

file_id.o:../../../common/linux/file_id.cc
	$(CPP) $(CPPFLAGS) -c $^

clean:
	rm -f $(BIN) $(DUMP_OBJ)
