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
# simple GNU Makefile for awk
# (Linux, Mac & Cygwin)
#
# Please see googleclient/native_client/common/Makefile.mk for details

# if nacl target, copy nexe to awk_module.nexe for awk.html to launch
ifeq (nacl,$(filter nacl,$(MAKECMDGOALS)))
POST_BUILD = cp $(EXE_NAME) awk_module.nexe
CC = $(NACL_BIN_PATH)/nacl-gcc

LIBS = -lgoogle_nacl_imc
NACL_INCLUDE = -I$(GOOGLE_CLIENT)/third_party/npapi/files/include

else
CCFLAGS=-DHAS_ISBLANK
# $(error This sample only works with nacl builds.)
endif
SUBSYSTEM :=

# CCFLAGS+=-Wall -Werror
NAME:=awk_module

AWK_FILES := ytab.c
AWK_FILES += b.c
AWK_FILES += main.c
AWK_FILES += parse.c
AWK_FILES += proctab.c
AWK_FILES += tran.c
AWK_FILES += lib.c
AWK_FILES += run.c
AWK_FILES += lex.c

AWK_SRC_DIR = ./awk_src/
AWK_SRC = $(addprefix $(AWK_SRC_DIR), $(AWK_FILES))

FILES:= $(AWK_SRC)

include ../../common/Makefile.mk

.PHONY: allclean download

allclean: clean
	rm -f ./*.nexe
	rm -rf $(AWK_SRC_DIR)


download:
	../../tools/download.sh http://www.cs.princeton.edu/~bwk/btl.mirror/awk.tar.gz awk.tar.gz
	mkdir awk_src
	cd awk_src ; tar xzf ../awk.tar.gz
	rm -f awk.tar.gz

