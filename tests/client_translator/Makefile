# Copyright 2010 The Native Client SDK Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
#
# simple GNU Makefile for translator demo
# (Linux only )

NAME:=translator
FILES:=translator.cc nacl_file.cc

NATIVE_CLIENT := $(subst /native_client,/native_client *,$(CURDIR))
NATIVE_CLIENT := $(filter-out *%,$(NATIVE_CLIENT))

# platform...
ifeq ($(shell uname -s), Darwin)
  # skip
else
  ENVIRON := $(shell uname -o)
  ifeq (Linux,$(findstring Linux,$(ENVIRON)))
    PLATFORM ?= linux
  else
  #skip
  endif
endif

NACL_BIN_PATH := $(NATIVE_CLIENT)/toolchain/$(PLATFORM)_x86/bin
CPP := $(NACL_BIN_PATH)/nacl64-g++

CCFLAGS=-Wall -Werror
CCFLAGS += -Xlinker --wrap -Xlinker read
CCFLAGS += -Xlinker --wrap -Xlinker write
CCFLAGS += -Xlinker --wrap -Xlinker close
CCFLAGS += -Xlinker --wrap -Xlinker lseek

OPT=-O3 -funroll-loops -fomit-frame-pointer

NACL_INCLUDE ?=
INCLUDE := -I$(NACL_INCLUDE)

EXE_NAME := $(NAME).nexe

SRC_FILES := $(FILES)

# include common libs that most demos want
LIBS += -lav -lsrpc -lpthread -lm

# build up all the various options
OPTIONS:=$(CCFLAGS) $(OPT) $(LIBS)

# targets

all: $(EXE_NAME)

clean:
	rm -f *.o *.nexe

$(EXE_NAME): $(SRC_FILES)
	$(CPP) $(SRC_FILES) $(INCLUDE) $(OPTIONS) -o $(EXE_NAME)

.PHONY: all clean
