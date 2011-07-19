# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Makefile to build the PPAPI Clang plugins

.PHONY: all clean

PLUGINS = PrintNamesAndSizes FindAffectedInterfaces

all: $(foreach plugin,$(PLUGINS),$(plugin))

clean: $(foreach plugin,$(PLUGINS),clean_$(plugin))

# This makefile assumes that you checked out llvm in a subdirectory of your
# home directory called 'llvm'.  The source of llvm itself should be one level
# deeper at ~/llvm/llvm.  It also assumes that you also have the clang component
# of llvm installed.
# It further assumes that clang has been built in Debug mode following the
# usual instructions, with a build directory parallel to the llvm directory.
#   See instructions here:  http://clang.llvm.org/get_started.html
# If you didn't configure Clang this way, you can set environment variables
# appropriately to work with your configuration of Clang and LLVM.
LLVM_ROOT ?= ~/llvm
LLVM_SOURCE_ROOT ?= $(LLVM_ROOT)/llvm
LLVM_BUILD_ROOT?=$(LLVM_ROOT)/build
LLVM_INCLUDES?=-I$(LLVM_BUILD_ROOT)/include -I$(LLVM_SOURCE_ROOT)/include
LLVM_LIB_PATHS?=-L$(LLVM_BUILD_ROOT)/Debug+Asserts/lib

CLANG_ROOT=$(LLVM_SOURCE_ROOT)/tools/clang
CLANG_BUILD_ROOT=$(LLVM_BUILD_ROOT)/tools/clang
CLANG_INCLUDES=-I$(CLANG_ROOT)/include -I$(CLANG_BUILD_ROOT)/include

INCLUDES=$(LLVM_INCLUDES) $(CLANG_INCLUDES) -I.

CLANG_DEFINITIONS=-D_DEBUG -D_GNU_SOURCE -D__STDC_LIMIT_MACROS\
 -D__STDC_CONSTANT_MACROS

COMMON_CCFLAGS=-fno-exceptions -fno-rtti -fPIC -Woverloaded-virtual -Wcast-qual\
 -fno-strict-aliasing -pedantic -Wno-long-long -Wall -W -Wno-unused-parameter\
 -Wwrite-strings

CCFLAGS=$(INCLUDES) $(CLANG_DEFINITIONS) $(COMMON_CCFLAGS) -O2
LDFLAGS = -Wl,-R $(LLVM_LIB_PATHS) -lpthread -ldl -lm -shared

CPP=g++

%.o: %.cc
	$(CPP) $(CCFLAGS) -c -o $@ $<

PRINT_NAMES_AND_SIZES_OBJECTS=print_names_and_sizes.o

PrintNamesAndSizes: $(PRINT_NAMES_AND_SIZES_OBJECTS)
	$(CPP) $^ $(LDFLAGS) -o libPrintNamesAndSizes.so

clean_PrintNamesAndSizes:
	rm -f $(PRINT_NAMES_AND_SIZES_OBJECTS) libPrintNamesAndSizes.so

FIND_AFFECTED_INTERFACES_OBJECTS=find_affected_interfaces.o

FindAffectedInterfaces: $(FIND_AFFECTED_INTERFACES_OBJECTS)
	$(CPP) $^ $(LDFLAGS) -o libFindAffectedInterfaces.so

clean_FindAffectedInterfaces:
	rm -f $(FIND_AFFECTED_INTERFACES_OBJECTS) libFindAffectedInterfaces.so


