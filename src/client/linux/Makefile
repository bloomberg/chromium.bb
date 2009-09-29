CXX=g++
CC=gcc

CXXFLAGS=-gstabs+ -I../../ -I../../testing/gtest/include -I../../testing -I../../third_party/linux/include -I../../testing/include -I../../testing/gtest -D_REENTRANT -m32 -Wall
CFLAGS=$(CXXFLAGS)
LDFLAGS=-lpthread

OBJ_DIR=.
BIN_DIR=.


# Source vars for components

# Google Test (meant to be included in each test source list)
GOOGLETEST_CC_SRC=../../testing/gtest/src/gtest_main.cc \
	          ../../testing/gtest/src/gtest-all.cc \
		  ../../testing/src/gmock-all.cc

# Google Logging
GOOGLELOG_LIB=../../third_party/linux/lib/glog/libglog.a

# Google Flags lib
GOOGLEFLAGS_LIB=../../third_party/linux/lib/gflags/libgflags.a

# Client exception handling & minidump writing library
LIB_CC_SRC=handler/exception_handler.cc \
	   minidump_writer/linux_dumper.cc \
	   minidump_writer/minidump_writer.cc \
	   ../minidump_file_writer.cc \
	   ../../common/string_conversion.cc \
	   ../../common/linux/guid_creator.cc
LIB_C_SRC = ../../common/convert_UTF.c
LIB_CC_OBJ=$(patsubst %.cc, $(OBJ_DIR)/%.o,$(LIB_CC_SRC))
LIB_C_OBJ=$(patsubst %.c, $(OBJ_DIR)/%.o, $(LIB_C_SRC))

# Unit tests for client library 
TEST_CC_SRC=handler/exception_handler_unittest.cc \
	    minidump_writer/directory_reader_unittest.cc \
	    minidump_writer/line_reader_unittest.cc \
	    minidump_writer/linux_dumper_unittest.cc \
	    minidump_writer/minidump_writer_unittest.cc \
	    $(GOOGLETEST_CC_SRC)

TEST_CC_OBJ=$(patsubst %.cc, $(OBJ_DIR)/%.o,$(TEST_CC_SRC))

# Library for crash dump uploader
SENDER_LIBRARY_SRC=../../common/linux/google_crashdump_uploader.cc ../../common/linux/libcurl_wrapper.cc
SENDER_LIBRARY_OBJ=$(patsubst %.cc, $(OBJ_DIR)/%.o,$(SENDER_LIBRARY_SRC))
SENDER_LDFLAGS=-ldl

# Unit test for crash dump uploader
SENDER_TEST_SRC=../../common/linux/google_crashdump_uploader_test.cc \
                $(GOOGLETEST_CC_SRC)

SENDER_TEST_OBJ=$(patsubst %.cc, $(OBJ_DIR)/%.o,$(SENDER_TEST_SRC))

# CLI tool for crash dump uploaer
SENDER_TOOL_SRC=sender/google_crash_report_sender.cc
SENDER_TOOL_OBJ=$(patsubst %.cc, $(OBJ_DIR)/%.o,$(SENDER_TOOL_SRC))

# Vars for binary targets
BREAKPAD_LIBRARY=$(BIN_DIR)/libbreakpad.a
SENDER_LIBRARY=$(BIN_DIR)/libcrash_sender.a
# Client unit test binary
UNITTEST_BIN=$(BIN_DIR)/linux_client_test
# Uploader unit test binary
SENDER_UNITTEST_BIN=$(BIN_DIR)/google_crashdump_uploader_test
# Sender CLI tool binary
SENDER_CLI_TOOL_BIN=$(BIN_DIR)/google_crashdump_uploader

.PHONY:all clean

all:$(BREAKPAD_LIBRARY) $(UNITTEST_BIN) $(SENDER_LIBRARY) $(SENDER_UNITTEST_BIN) $(SENDER_CLI_TOOL_BIN)

check:$(UNITTEST_BIN) $(SENDER_UNITTEST_BIN)
	$(UNITTEST_BIN)
	$(SENDER_UNITTEST_BIN)

$(BIN_DIR)/libbreakpad.a:$(LIB_CC_OBJ) $(LIB_C_OBJ)
	$(AR) rcs $@ $^

$(BIN_DIR)/linux_client_test:$(TEST_CC_OBJ) $(BREAKPAD_LIBRARY)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

$(BIN_DIR)/libcrash_sender.a:$(SENDER_LIBRARY_OBJ)
	$(AR) rcs $@ $^

$(BIN_DIR)/google_crashdump_uploader_test:$(SENDER_TEST_OBJ) $(SENDER_LIBRARY) \
	$(GOOGLELOG_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SENDER_LDFLAGS) $^ -o $@

$(BIN_DIR)/google_crashdump_uploader:$(SENDER_TOOL_OBJ) $(SENDER_LIBRARY) \
	$(GOOGLELOG_LIB) $(GOOGLEFLAGS_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SENDER_LDFLAGS) $^ -o $@

clean:
	rm -f $(UNITTEST_BIN) $(BREAKPAD_LIBRARY) $(LIB_CC_OBJ) $(LIB_C_OBJ) \
	$(TEST_CC_OBJ) $(SENDER_LIBRARY_OBJ) $(SENDER_LIBRARY) \
	$(SENDER_TOOL_OBJ) $(SENDER_CLI_TOOL_BIN) $(SENDER_UNITTEST_BIN) core
