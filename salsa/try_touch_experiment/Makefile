# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

OBJDIR = obj

OBJECTS=\
	$(OBJDIR)/try_touch_experiment.o \
	$(OBJDIR)/salsa_experiment_runner.o \
	$(OBJDIR)/treatment.o \
	$(OBJDIR)/property.o \
	$(OBJDIR)/experiment.o

DESTDIR = .

CXXFLAGS+=\
	-g \
	-fno-exceptions \
	-fno-strict-aliasing \
	-fPIC \
	-Wall \
	-Wclobbered \
	-Wempty-body \
	-Werror \
	-Wignored-qualifiers \
	-Wmissing-field-initializers \
	-Wsign-compare \
	-Wtype-limits \
	-Wuninitialized \
	-D__STDC_FORMAT_MACROS=1 \
	-D_FILE_OFFSET_BITS=64 \
	-DGESTURES_INTERNAL=1 \
	-I..

PKG_CONFIG ?= pkg-config
BASE_VER ?= 180609
PC_DEPS = libchrome-$(BASE_VER)
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CXXFLAGS += $(PC_CFLAGS)

LINK_FLAGS=\
	$(PC_LIBS) \
	-lcurses

EXE=try_touch_experiment

all: $(EXE)

$(EXE): $(OBJECTS)
	$(CXX) -o $@ $(CXXFLAGS) $(OBJECTS) $(LINK_FLAGS)

$(OBJDIR)/%.o : %.cc
	mkdir -p $(OBJDIR) || true
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR)/%.o : %.c
	mkdir -p $(OBJDIR) || true
	$(CC) $(CFLAGS) -c -o $@ $<

install: $(EXE)
	install -D -m 0755 $(EXE) $(DESTDIR)/usr/sbin/$(EXE)

clean:
	rm -rf $(OBJDIR) $(EXE)

.PHONY : clean all
