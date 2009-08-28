# Copyright 2009, Google Inc.
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
# simple Makefile for Native Client drawing demo
# (Linux, Mac & Cygwin)
#
# Please see googleclient/native_client/common/Makefile.mk for details
#
# type:
#
#   make download nacl
# to download and build the Anti-Grain Geometry library for Native Client.
#
#   make debug nacl run
# to build the demo and run it as a Native Client application.
#
#   make agg nacl
# to build Anti-Grain Geometry's libagg.a without re-downloading
#
# This file and the sources expect to be in:
#   googleclient/native_client/tests/drawing

NAME:=drawing

ifeq (release,$(filter release,$(MAKECMDGOALS)))
  OPT := -O2
else
  OPT := -O0 -g
endif

#if nacl target, copy nexe to drawing.nexe for drawing.html to launch
ifeq (nacl,$(filter nacl,$(MAKECMDGOALS)))
  POST_BUILD = cp $(EXE_NAME) drawing.nexe
else
  $(warning This demo requires 'nacl' as a build target.)
  $(warning For example, 'make release nacl')
  $(warning See native_client/common/Makefile.mk for all build options.)
  $(error Unable to continue)
endif

FILES := drawing.cc
LIBS := -L./ -lagg

include ../../common/Makefile.mk

.PHONY: download agg

ANTIGRAIN_URL=http://www.antigrain.com/
download:
	# remove previous agg (if applicable)
	rm -rf agg-2.5
	../../tools/download.sh $(ANTIGRAIN_URL)/agg-2.5.tar.gz agg-2.5.tar.gz
	tar -x -f agg-2.5.tar.gz
	# clean up
	rm -rf agg-2.5.tar.gz
	# apply Native Client patch
	#cp Makefile.in.NativeClient agg-2.5/.
	#cp Makefile.ag agg-2.5/src/Makefile
	cd agg-2.5 ; patch -p1 <../nacl_agg-2.5.patch
	cd agg-2.5 ; make NACL_BIN_PATH=$(NACL_BIN_PATH)
	cp agg-2.5/src/libagg.a .

agg:
	cd agg-2.5 ; make NACL_BIN_PATH=$(NACL_BIN_PATH)
	cp agg-2.5/src/libagg.a .

