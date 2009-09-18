CXX=g++
CC=gcc

CXXFLAGS=-gstabs+ -I../../ -I../../testing/gtest/include -I../../testing/include -I../../testing/gtest -D_REENTRANT -m32
CFLAGS=$(CXXFLAGS)
LDFLAGS=-lpthread

OBJ_DIR=.
BIN_DIR=.

LIB_CC_SRC=handler/exception_handler.cc \
	   minidump_writer/linux_dumper.cc \
	   minidump_writer/minidump_writer.cc \
	   ../minidump_file_writer.cc \
	   ../../common/string_conversion.cc \
	   ../../common/linux/guid_creator.cc

LIB_C_SRC = ../../common/convert_UTF.c

LIB_CC_OBJ=$(patsubst %.cc, $(OBJ_DIR)/%.o,$(LIB_CC_SRC))
LIB_C_OBJ=$(patsubst %.c, $(OBJ_DIR)/%.o, $(LIB_C_SRC))

TEST_CC_SRC=handler/exception_handler_unittest.cc \
	    minidump_writer/directory_reader_unittest.cc \
	    minidump_writer/line_reader_unittest.cc \
	    minidump_writer/linux_dumper_unittest.cc \
	    minidump_writer/minidump_writer_unittest.cc \
	    ../../testing/gtest/src/gtest_main.cc \
	    ../../testing/gtest/src/gtest-all.cc

TEST_CC_OBJ=$(patsubst %.cc, $(OBJ_DIR)/%.o,$(TEST_CC_SRC))

UNITTEST_BIN=$(BIN_DIR)/linux_client_test

BREAKPAD_LIBRARY=$(BIN_DIR)/libbreakpad.a

.PHONY:all clean

all:$(BREAKPAD_LIBRARY) $(UNITTEST_BIN)

check:$(UNITTEST_BIN)
	$(UNITTEST_BIN)

$(BIN_DIR)/libbreakpad.a:$(LIB_CC_OBJ) $(LIB_C_OBJ)
	$(AR) rcs $@ $^

$(BIN_DIR)/linux_client_test:$(TEST_CC_OBJ) $(BREAKPAD_LIBRARY)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

clean:
	rm -f $(UNITTEST_BIN) $(BREAKPAD_LIBRARY) $(LIB_CC_OBJ) $(LIB_C_OBJ) $(TEST_CC_OBJ) core
