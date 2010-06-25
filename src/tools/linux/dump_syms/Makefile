# Build all executables.
all::

# Delete all generated files: executables, object files, test coverage
# reports, etc.
clean::

# Run all tests.
# You can run a specific test FOO with the command: 'make check-FOO'.
check:

# Generate coverage reports for the execution that has taken place
# since the coverage data files were last deleted. Only files that
# have been added to COVERAGE_SOURCES (see below) get reported on.
coverage:

# Reset all coverage counts. When coverage is enabled, each time you
# run the program, it adds its execution counts into the profiling
# data files in the build directory. 'make coverage-reset' deletes
# those files, so the counts reported by 'make coverage' start from
# zero again. Note that 'make clean' does this automatically.
coverage-reset:

.PHONY: all clean check coverage coverage-reset

### Variables that are useful to override on the command line.
CC = gcc
CXX = g++
CXXFLAGS = -g3 -O2 -Wall -m32

# To produce test coverage reports:
# 1) Build all .o files and executables without optimization and with
#    'COVERAGE=1' on the make command line.
# 2) Run the tests.
# 3) Do 'make coverage'.
# All in one command:
# $ make CFLAGS='-O0' CXXFLAGS='-O0' COVERAGE=1 clean check coverage
COVERAGE=

# A wrapper for running 'make check' targets. This is inserted before
# the test executable name in the commands for 'check' targets. So if
# 'make check' would normally execute some test program with the
# command:
#
#    SOME_ENV_VAR=value ./some-test-executable test-args
#
# then setting TEST_WRAPPER to 'valgrind' (say) would have 'make
# check' run the this command line:
#
#    SOME_ENV_VAR=value valgrind ./some-test-executable test-args
TEST_WRAPPER=

# Arguments to pass to the test programs.
TEST_ARGS=

### Variables used internally by this Makefile.

# The top of the breakpad source tree.
SRC = ../../..

# A list of the executables that we should use the C++ compiler to
# link. GNU make's default executable-from-object rule uses $(CC),
# which doesn't include libstdc++, and perhaps does some other things
# wrong as well. Every executable listed in this variable uses the
# pattern rule provided at the bottom, which links all the
# dependencies using $(CXX). Value accumulated throughout the file.
CPP_EXECUTABLES = 

# Add the names of source files whose coverage you'd like 'make
# coverage' to report on to this variable. Value accumulated
# throughout the file.
COVERAGE_SOURCES = 


### dump_syms: a program to produce Breakpad symbol files from the
### debugging information in Linux executables.
all:: dump_syms
dump_syms:					\
	bytereader.o				\
	dump_symbols.o				\
	dump_syms.o				\
	dwarf2diehandler.o			\
	dwarf2reader.o				\
	dwarf_cfi_to_module.o			\
	dwarf_cu_to_module.o			\
	dwarf_line_to_module.o			\
	file_id.o				\
	language.o				\
	module.o				\
	stabs_reader.o				\
	stabs_to_module.o			\
	$(empty)
CPP_EXECUTABLES += dump_syms
clean::
	rm -f dump_syms

dump_syms.o: dump_syms.cc

VPATH += $(SRC)/common
dwarf_cfi_to_module.o: dwarf_cfi_to_module.cc
COVERAGE_SOURCES += dwarf_cfi_to_module.cc
dwarf_cu_to_module.o: dwarf_cu_to_module.cc
COVERAGE_SOURCES += dwarf_cu_to_module.cc
dwarf_line_to_module.o: dwarf_line_to_module.cc
COVERAGE_SOURCES += dwarf_line_to_module.cc
language.o: language.cc
module.o: module.cc
COVERAGE_SOURCES += module.cc
stabs_reader.o: stabs_reader.cc
COVERAGE_SOURCES += stabs_reader.cc
stabs_to_module.o: stabs_to_module.cc
COVERAGE_SOURCES += stabs_to_module.cc

VPATH += $(SRC)/common/linux
dump_symbols.o: dump_symbols.cc
file_id.o: file_id.cc

VPATH += $(SRC)/common/dwarf
bytereader.o: bytereader.cc
COVERAGE_SOURCES += bytereader.cc
cfi_assembler.o: cfi_assembler.cc
dwarf2diehandler.o: dwarf2diehandler.cc
COVERAGE_SOURCES += dwarf2diehandler.cc
dwarf2reader.o: dwarf2reader.cc
COVERAGE_SOURCES += dwarf2reader.cc


### Google C++ Testing Framework.
VPATH += $(SRC)/testing/gtest/src
GTEST_CPPFLAGS = -I$(SRC)/testing/gtest/include -I$(SRC)/testing/gtest
gtest-all.o: gtest-all.cc
gtest_main.o: gtest_main.cc
gtest-all.o gtest_main.o: override CPPFLAGS += $(GTEST_CPPFLAGS)


### Google C++ Mocking Framework.
VPATH += $(SRC)/testing/src
GMOCK_CPPFLAGS = -I$(SRC)/testing -I$(SRC)/testing/include
gmock-all.o: gmock-all.cc
gmock-all.o: override CPPFLAGS += $(GTEST_CPPFLAGS) $(GMOCK_CPPFLAGS)


### google_breakpad::TestAssembler, for constructing binary test data
VPATH += $(SRC)/common
test_assembler.o: test_assembler.cc
test_assembler.o: override CPPFLAGS += $(TEST_ASSEMBLER_CPPFLAGS)


### Unit tests for google_breakpad::StabsReader.
check: check-stabs_reader_unittest
check-stabs_reader_unittest: stabs_reader_unittest
stabs_reader_unittest:				\
	gmock-all.o				\
	gtest-all.o				\
	gtest_main.o				\
	stabs_reader.o				\
	test_assembler.o			\
	$(empty)
CPP_EXECUTABLES += stabs_reader_unittest
stabs_reader_unittest.o: stabs_reader_unittest.cc
stabs_reader_unittest.o: override CPPFLAGS += $(GTEST_CPPFLAGS) \
					      $(GMOCK_CPPFLAGS)
clean::
	rm -f stabs_reader_unittest

### Unit tests for google_breakpad::FileID.
check: check-file_id_unittest
check-file_id_unittest: file_id_unittest
file_id_unittest:				\
	file_id.o				\
	gmock-all.o				\
	gtest-all.o				\
	gtest_main.o				\
	$(empty)
CPP_EXECUTABLES += file_id_unittest
file_id_unittest.o: file_id_unittest.cc
file_id_unittest.o: override CPPFLAGS += $(GTEST_CPPFLAGS) \
				         $(GMOCK_CPPFLAGS)
clean::
	rm -f file_id_unittest


### Unit tests for google_breakpad::Module.
check: check-module_unittest
check-module_unittest: module_unittest
module_unittest:				\
	gmock-all.o				\
	gtest-all.o				\
	gtest_main.o				\
	module.o				\
	module_unittest.o			\
	$(empty)
CPP_EXECUTABLES += module_unittest
module_unittest.o: module_unittest.cc
module_unittest.o: override CPPFLAGS += $(GTEST_CPPFLAGS) $(GMOCK_CPPFLAGS)
clean::
	rm -f module_unittest


### Unit tests for google_breakpad::DumpStabsHandler.
check: check-stabs_to_module_unittest
check-stabs_to_module_unittest: stabs_to_module_unittest
stabs_to_module_unittest:				\
	gtest-all.o					\
	gtest_main.o					\
	module.o					\
	stabs_to_module.o				\
	stabs_to_module_unittest.o			\
	$(empty)
CPP_EXECUTABLES += stabs_to_module_unittest
stabs_to_module_unittest.o: stabs_to_module_unittest.cc
stabs_to_module_unittest.o: override CPPFLAGS += $(GTEST_CPPFLAGS) \
						 $(GMOCK_CPPFLAGS)
clean::
	rm -f stabs_to_module_unittest


### Unit tests for dwarf2reader::DwarfDIEDispatcher.
check: check-dwarf2diehandler_unittest
check-dwarf2diehandler_unittest: dwarf2diehandler_unittest
dwarf2diehandler_unittest:			\
	dwarf2diehandler.o			\
	gmock-all.o				\
	gtest-all.o				\
	gtest_main.o				\
	$(empty)
CPP_EXECUTABLES += dwarf2diehandler_unittest
dwarf2diehandler_unittest.o: dwarf2diehandler_unittest.cc
dwarf2diehandler_unittest.o: override CPPFLAGS += $(GTEST_CPPFLAGS) \
						  $(GMOCK_CPPFLAGS)
clean::
	rm -f dwarf2diehandler_unittest



### Unit tests for google_breakpad::DwarfLineToModule.
check: check-dwarf_line_to_module_unittest
check-dwarf_line_to_module_unittest: dwarf_line_to_module_unittest
dwarf_line_to_module_unittest:			\
	dwarf_line_to_module.o			\
	gtest-all.o				\
	gtest_main.o				\
	module.o				\
	$(empty)
CPP_EXECUTABLES += dwarf_line_to_module_unittest
dwarf_line_to_module_unittest.o: dwarf_line_to_module_unittest.cc
dwarf_line_to_module_unittest.o: override CPPFLAGS += $(GTEST_CPPFLAGS) \
						      $(GMOCK_CPPFLAGS)
clean::
	rm -f dwarf_line_to_module_unittest



### Unit tests for google_breakpad::DwarfCUToModule.
check: check-dwarf_cu_to_module_unittest
check-dwarf_cu_to_module_unittest: dwarf_cu_to_module_unittest
dwarf_cu_to_module_unittest:      		\
	dwarf_cu_to_module.o			\
	gmock-all.o				\
	gtest-all.o				\
	gtest_main.o				\
	language.o				\
	module.o				\
	$(empty)
CPP_EXECUTABLES += dwarf_cu_to_module_unittest
dwarf_cu_to_module_unittest.o: dwarf_cu_to_module_unittest.cc
dwarf_cu_to_module_unittest.o: override CPPFLAGS += $(GTEST_CPPFLAGS) \
						    $(GMOCK_CPPFLAGS)
clean::
	rm -f dwarf_cu_to_module_unittest



### Unit tests for dwarf2reader::CallFrameInfo.
check: check-dwarf2reader_cfi_unittest
check-dwarf2reader_cfi_unittest: dwarf2reader_cfi_unittest
dwarf2reader_cfi_unittest:      		\
	bytereader.o				\
	cfi_assembler.o				\
	dwarf2reader.o				\
	gmock-all.o				\
	gtest-all.o				\
	gtest_main.o				\
	test_assembler.o			\
	$(empty)
CPP_EXECUTABLES += dwarf2reader_cfi_unittest
dwarf2reader_cfi_unittest.o: dwarf2reader_cfi_unittest.cc
dwarf2reader_cfi_unittest.o: override CPPFLAGS += $(GTEST_CPPFLAGS) \
						  $(GMOCK_CPPFLAGS) \
						  $(TEST_ASSEMBLER_CPPFLAGS)
clean::
	rm -f dwarf2reader_cfi_unittest



### Unit tests for google_breakpad::DwarfCFIToModule.
check: check-dwarf_cfi_to_module_unittest
check-dwarf_cfi_to_module_unittest: dwarf_cfi_to_module_unittest
dwarf_cfi_to_module_unittest:      		\
	dwarf_cfi_to_module.o			\
	gmock-all.o				\
	gtest-all.o				\
	gtest_main.o				\
	module.o				\
	$(empty)
CPP_EXECUTABLES += dwarf_cfi_to_module_unittest
dwarf_cfi_to_module_unittest.o: dwarf_cfi_to_module_unittest.cc
dwarf_cfi_to_module_unittest.o: override CPPFLAGS += $(GTEST_CPPFLAGS) \
						     $(GMOCK_CPPFLAGS)
clean::
	rm -f dwarf_cfi_to_module_unittest



### Unit tests for google_breakpad::ByteReader
check: check-bytereader_unittest
check-bytereader_unittest: bytereader_unittest
bytereader_unittest:				\
	bytereader.o				\
	cfi_assembler.o				\
	gtest-all.o				\
	gtest_main.o				\
	test_assembler.o			\
	$(empty)
CPP_EXECUTABLES += bytereader_unittest
bytereader_unittest.o: bytereader_unittest.cc
bytereader_unittest.o: override CPPFLAGS += $(GTEST_CPPFLAGS) \
					    $(GMOCK_CPPFLAGS)
clean::
	rm -f bytereader_unittest



### Unit tests for google_breakpad::ByteBuffer and google_breakpad::ByteCursor.
check: check-byte_cursor_unittest
check-byte_cursor_unittest: byte_cursor_unittest
byte_cursor_unittest:				\
	gmock-all.o				\
	gtest-all.o				\
	gtest_main.o				\
	$(empty)
CPP_EXECUTABLES += byte_cursor_unittest
VPATH += $(SRC)/common
byte_cursor_unittest.o: byte_cursor_unittest.cc
byte_cursor_unittest.o: override CPPFLAGS += $(GTEST_CPPFLAGS) \
					     $(GMOCK_CPPFLAGS)
COVERAGE_SOURCES += byte_cursor_unittest.cc
clean::
	rm -f byte_cursor_unittest



### Generic compilation rules.

# Link C++ executables using the C++ compiler; see CPP_EXECUTABLES above.
$(CPP_EXECUTABLES): %: %.o
	$(CXX) $(CXXFLAGS) $(COVERAGE_LDFLAGS) $(LDFLAGS) -o $@ $^

# Allow #include directives to refer to files below 'src'; generate
# dependency files automatically; and I doubt _REENTRANT is needed at all.
BREAKPAD_CPPFLAGS = -I$(SRC) -MMD -D_REENTRANT -DHAVE_A_OUT_H

# Bring in whatever dependency files we have generated by compiling with -MMD.
-include *.d

%.o: %.cc
	$(CXX) -c $< -o $@ $(CPPFLAGS) $(BREAKPAD_CPPFLAGS) $(CXXFLAGS)
%.o: %.c
	$(CC)  -c $< -o $@ $(CPPFLAGS) $(BREAKPAD_CPPFLAGS) $(CFLAGS)

clean::
	rm -f *.o *.d core

### Generic testing rules.

### To define a test, make the 'check' target depend on a particular
### target 'check-FOO' that runs your test, where FOO is the name of
### the test executable, or something else people will expect.
### 
### This pattern rule provides commands for 'check-FOO' that are
### appropriate for Google C++ Testing Framework test programs. But
### you can provide your own commands.
check-%: %
	$(TEST_WRAPPER) ./$< $(TEST_ARGS)


### Generic coverage reporting rules.
coverage:
	gcov --branch-probabilities $(COVERAGE_SOURCES)

coverage-reset:
	rm -f *.gcda

# If code coverage is enabled, pass the appropriate coverage flags to
# the compiler for the sources we care about.
ifdef COVERAGE

COVERAGE_C_SOURCES = $(filter %.c,$(COVERAGE_SOURCES))
$(COVERAGE_C_SOURCES:.c=.o): override CFLAGS += --coverage

COVERAGE_CXX_SOURCES = $(filter %.cc,$(COVERAGE_SOURCES))
$(COVERAGE_CXX_SOURCES:.cc=.o): override CXXFLAGS += --coverage

COVERAGE_LDFLAGS = --coverage

endif

clean:: coverage-reset
clean::
	rm -f *.gcno *.gcov
