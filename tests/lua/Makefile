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
# simple GNU Makefile for lua demo
# (Linux, Mac & Cygwin)
#
# Please see googleclient/native_client/common/Makefile.mk for details

# if nacl target, copy nexe to lua_module.nexe for lua.html to launch
ifeq (nacl,$(filter nacl,$(MAKECMDGOALS)))
POST_BUILD = cp $(EXE_NAME) lua_module.nexe
CC = $(NACL_BIN_PATH)/nacl-gcc
else
$(error This sample only works with nacl builds.)
endif
SUBSYSTEM :=
LUA_DIR := lua-5.1.4

CCFLAGS=-Wall -Werror
NAME:=lua_module

LIBS = -lgoogle_nacl_npruntime
LIBS+= -lgoogle_nacl_imc
NACL_INCLUDE = -I$(GOOGLE_CLIENT)/third_party/npapi/files/include

LUA_FILES := lapi.c
LUA_FILES += lcode.c
LUA_FILES += ldebug.c
LUA_FILES += ldo.c
LUA_FILES += ldump.c
LUA_FILES += lfunc.c
LUA_FILES += lgc.c
LUA_FILES += llex.c
LUA_FILES += lmem.c
LUA_FILES += lobject.c
LUA_FILES += lopcodes.c
LUA_FILES += lparser.c
LUA_FILES += lstate.c
LUA_FILES += lstring.c
LUA_FILES += ltable.c
LUA_FILES += ltm.c
LUA_FILES += lundump.c
LUA_FILES += lvm.c
LUA_FILES += lzio.c
LUA_FILES += lauxlib.c
LUA_FILES += lbaselib.c
LUA_FILES += ldblib.c
LUA_FILES += lmathlib.c
LUA_FILES += ltablib.c
LUA_FILES += lstrlib.c
LUA_OBJS = $(addprefix $(LUA_DIR)/src/, $(patsubst %.c, %.o, $(LUA_FILES)))

FILES:=lua_module.cc $(LUA_OBJS)

include ../../common/Makefile.mk

$(LUA_OBJS): %.o: %.c
	$(CC) $(INCLUDE) $(CCFLAGS) $(OPT) $(XRAY_OPT) $(DBG) -c $< -o $@

.PHONY: allclean download


allclean: clean
	rm -rf ./lua-5.1.4
	rm -f ./*.nexe
	rm -f $(LUA_OBJS)

download:
	../../tools/download.sh http://www.lua.org/ftp/lua-5.1.4.tar.gz lua.tgz
	rm -rf lua-5.1.4
	tar zxf lua.tgz
	rm -f lua.tgz
