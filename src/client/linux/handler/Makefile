CXX=g++
CC=gcc

CXXFLAGS=-gstabs+ -I../../../ -I../../../testing/gtest/include -I../../../testing/include -I../../../testing/gtest -D_REENTRANT -m32
CFLAGS=$(CXXFLAGS)
LDFLAGS=-lpthread

OBJ_DIR=.
BIN_DIR=.

TEST_CC_SRC=exception_handler_unittest.cc \
	    exception_handler.cc \
	    ../../../testing/gtest/src/gtest-all.cc \
	    ../../../common/linux/guid_creator.cc \
	    ../minidump_writer/minidump_writer.cc \
	    ../../minidump_file_writer.cc \
	    ../minidump_writer/linux_dumper.cc \
	    ../../../testing/gtest/src/gtest_main.cc \
	    ../../../common/string_conversion.cc \
	    ../minidump_writer/directory_reader_unittest.cc \
	    ../minidump_writer/line_reader_unittest.cc \
	    ../minidump_writer/linux_dumper_unittest.cc \
	    ../minidump_writer/minidump_writer_unittest.cc

TEST_C_SRC = ../../../common/convert_UTF.c

TEST_CC_OBJ=$(patsubst %.cc, $(OBJ_DIR)/%.o,$(TEST_CC_SRC))
TEST_C_OBJ=$(patsubst %.c, $(OBJ_DIR)/%.o, $(TEST_C_SRC))

LINUX_CLIENT_BIN=$(BIN_DIR)/linux_client_test

BIN=$(LINUX_CLIENT_BIN)

.PHONY:all clean

check:$(BIN)
	$(LINUX_CLIENT_BIN)

all:$(BIN)

$(BIN_DIR)/linux_client_test:$(TEST_CC_OBJ) $(TEST_C_OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

clean:
	rm -f $(BIN) $(TEST_CC_OBJ) $(TEST_C_OBJ)
