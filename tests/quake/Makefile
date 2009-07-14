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
# simple Makefile for Native Client Quake demo
# (Linux, Mac & Cygwin)
#
# Please see googleclient/native_client/common/Makefile.mk for details
#
# type:
#   make debug nacl run
# to build Quake and run it as a Native Client application.
#
# This file and the sources expect to be in googleclient/native_client/tests/quake

NAME:=naclquake
NACL_CPP:=nacl-gcc
STANDALONE_CPP:=gcc -lstdc++
CCFLAGS := -D_REENTRANT -DSDL -DELF -DPACKAGE=\"naclquake\" -DVERSION=\"1.0.9\" -I.

# if nacl target, copy nexe to "nacl_quake" for quake.html to launch
# and add nacl_file.c to list of files for browser support.
ifeq (nacl,$(filter nacl,$(MAKECMDGOALS)))
POST_BUILD = cp $(EXE_NAME) nacl_quake
FILES := nacl_file.c
else
FILES :=
endif

ifeq (release,$(filter release,$(MAKECMDGOALS)))
OPT := -O2
else
OPT := -O0 -g
endif

X86_SRCS :=  snd_mixa.S sys_dosa.S d_draw.S d_draw16.S d_parta.S d_polysa.S\
             d_scana.S d_spr8.S d_varsa.S math.S r_aclipa.S r_aliasa.S\
             r_drawa.S r_edgea.S surf16.S surf8.S worlda.S

NONX86_SRCS :=  d_vars.c nonintel.c

FILES += cd_nacl.c chase.c cl_demo.c cl_input.c cl_main.c cl_parse.c\
         cl_tent.c cmd.c common.c console.c crc.c cvar.c d_edge.c d_fill.c\
         d_init.c d_modech.c d_part.c d_polyse.c d_scan.c d_sky.c d_sprite.c\
         d_surf.c d_zpoint.c draw.c host.c host_cmd.c keys.c mathlib.c menu.c\
         model.c net_bsd.c net_dgrm.c net_loop.c net_main.c net_udp.c\
         net_vcr.c net_wso.c pr_cmds.c pr_edict.c pr_exec.c r_aclip.c\
         r_alias.c r_bsp.c r_draw.c r_edge.c r_efrag.c r_light.c r_main.c\
         r_misc.c r_part.c r_sky.c r_sprite.c r_surf.c r_vars.c r_varsa.S\
         sbar.c screen.c snd_dma.c snd_mem.c snd_mix.c snd_nacl.c sv_main.c\
         sv_move.c sv_phys.c sv_user.c sys_nacl.c vid_nacl.c view.c wad.c\
         world.c zone.c $(X86_SRCS) $(NONX86_SRCS)
# These files were excluded from FILES because the use instructions
# disallowed by Native Client
OMITTED_NACL_FILES := d_copy.S dosasm.S

include ../../common/Makefile.mk

.PHONY: download cleanall

QUAKE_URL=http://www.libsdl.org/projects/quake
download:
	../../tools/download.sh $(QUAKE_URL)/src/sdlquake-1.0.9.tar.gz quake_src.tgz
	../../tools/download.sh $(QUAKE_URL)/data/quakesw-1.0.6.tar.gz quake_data.tgz
	tar -x --strip-components=1 -f quake_src.tgz
	tar -x -f quake_data.tgz
	# apply Native Client patch
	patch -p1 <nacl_quake.patch
	# clean up - remove the tarballs
	rm -rf quake_src.tgz
	rm -rf quake_data.tgz


allclean: clean
	# clean out source except for nacl_file.c
	find . -name "*.c" !  -name "nacl_file.c"   -exec "rm" "{}" ";"
	# remove all headers and other stuff, if any
	rm -f  ./*.h  ./*.S WinQ*  *.bat conf*
