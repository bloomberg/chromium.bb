# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

OBJDIR = obj

OBJECTS=\
	$(OBJDIR)/touch_noise_monitor_main.o

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
	-Wno-missing-format-attribute \
	-D__STDC_FORMAT_MACROS=1 \
	-D_FILE_OFFSET_BITS=64 \
	-DGESTURES_INTERNAL=1 \
	-I..

PKG_CONFIG ?= pkg-config
BASE_VER ?= 125070
PC_DEPS = libchrome-$(BASE_VER)
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CXXFLAGS += $(PC_CFLAGS)

LINK_FLAGS=\
	$(PC_LIBS) \
	-lgflags \
	-lpthread \
	-lrt \
	-ludev \
	-levdev

EXE=touch_noise_monitor

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
