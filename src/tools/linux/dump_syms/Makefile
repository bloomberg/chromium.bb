### Variables that can be overridden on the command line.
CC = gcc
CXX = g++
CPPFLAGS = -DNDEBUG
CXXFLAGS = -g3 -O2 -Wall -m32

# To produce test coverage reports:
# 1) Build all .o files and executables without optimization and with
#    'COVERAGE=1' on the make command line.
# 2) Run the tests.
# 3) Do 'make coverage'.
# All in one command:
# $ make CFLAGS='-O0' CXXFLAGS='-O0' COVERAGE=1 clean check coverage
COVERAGE=

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

# Source files whose coverage we are interested in. Value accumulated
# throughout the file.
COVERAGE_SOURCES = 


### dump_syms: a program to produce Breakpad symbol files from the
### debugging information in Linux executables.
all:: dump_syms
dump_syms:					\
	dump_syms.o				\
	dump_symbols.o				\
	file_id.o				\
	module.o				\
	stabs_reader.o				\
	$(empty)
CPP_EXECUTABLES += dump_syms
clean::
	rm -f dump_syms

dump_syms.o: dump_syms.cc

VPATH += $(SRC)/common/linux
dump_symbols.o: dump_symbols.cc
file_id.o: file_id.cc
module.o: module.cc
stabs_reader.o: stabs_reader.cc
COVERAGE_SOURCES += stabs_reader.cc



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


### Unit tests for google_breakpad::StabsReader.
check: check-stabs_reader_unittest
check-stabs_reader_unittest: stabs_reader_unittest
stabs_reader_unittest:				\
	gmock-all.o				\
	gtest-all.o				\
	gtest_main.o				\
	stabs_reader.o				\
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
	gmock-all.o				\
	gtest-all.o				\
	gtest_main.o				\
	file_id.o				\
	$(empty)
CPP_EXECUTABLES += file_id_unittest
file_id_unittest.o: file_id_unittest.cc
file_id_unittest.o: override CPPFLAGS += $(GTEST_CPPFLAGS) \
				         $(GMOCK_CPPFLAGS)
clean::
	rm -f file_id_unittest


### Generic compilation rules.

# Link C++ executables using the C++ compiler; see CPP_EXECUTABLES above.
$(CPP_EXECUTABLES): %: %.o
	$(CXX) $(CXXFLAGS) $(COVERAGE_LDFLAGS) $(LDFLAGS) -o $@ $^

# These flags are required for breakpad sources to compile correctly.
BREAKPAD_CPPFLAGS = -I$(SRC) -D_REENTRANT

%.o: %.cc
	$(CXX) -c $^ -o $@ $(CPPFLAGS) $(BREAKPAD_CPPFLAGS) $(CXXFLAGS)
%.o: %.c
	$(CC)  -c $^ -o $@ $(CPPFLAGS) $(BREAKPAD_CPPFLAGS) $(CFLAGS)

clean::
	rm -f *.o core

### Generic testing rules.

### To define a test, make the 'check' target depend on a particular
### target 'check-FOO' that runs your test, where FOO is the name of
### the test executable, or something else people will expect.
### 
### This pattern rule provides commands for 'check-FOO' that are
### appropriate for Google C++ Testing Framework test programs. But
### you can provide your own commands.
check-%: %
	srcdir=$(SRC) ./$< $(TEST_ARGS)


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
