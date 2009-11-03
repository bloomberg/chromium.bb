# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#
# simple GNU Makefile for photo demo
# (Linux, Mac & Cygwin)
#
# Please see native_client/common/Makefile.mk for details

CCFLAGS=-Wall -Werror

# if nacl target, copy nexe to photo.nexe for photo.html to launch
ifeq (nacl,$(filter nacl,$(MAKECMDGOALS)))
  CCFLAGS += -Xlinker --wrap -Xlinker read
  CCFLAGS += -Xlinker --wrap -Xlinker open
  CCFLAGS += -Xlinker --wrap -Xlinker close
  CCFLAGS += -Xlinker --wrap -Xlinker lseek
  POST_BUILD = cp $(EXE_NAME) photo.nexe
else
  $(warning This demo requires 'nacl' as a build target.)
  $(warning For example, 'make release nacl')
  $(warning See native_client/common/Makefile.mk for all build options.)
  $(error Unable to continue)
endif

# photo example needs sse option for intrinsics
CCFLAGS+=-mfpmath=sse -msse3

ifeq (release,$(filter release,$(MAKECMDGOALS)))
  OPT=-O3 -funroll-loops -fomit-frame-pointer
endif

NAME:=photo
FILES:=photo.cc surface.cc nacl_file.cc
LIBS:=-L./ -ljpeg
CLIENT:=$(filter-out *%,$(subst /native_client,/native_client *,$(CURDIR)))

include $(CLIENT)/common/Makefile.mk
