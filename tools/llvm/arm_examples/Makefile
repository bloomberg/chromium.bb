# Copyright 2009 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
# Copyright 2009, Google Inc.

# ======================================================================
# Boilerplate
# ======================================================================
SDK_ROOT=/usr/local/crosstool

SEL_LDR=$(SDK_ROOT)/tools-arm/sel_ldr

QEMU_SCRIPT=$(SDK_ROOT)/qemu-arm/qemu_tool.sh

NACL_CC=$(SDK_ROOT)/arm-none-linux-gnueabi/llvm-fake-sfigcc \
        -std=gnu99 \
        -Wall \
        -Werror \
        -nostdinc \
        -D__native_client__=1 \
        -DNACL_TARGET_ARCH=arm \
        -DNACL_TARGET_SUBARCH=32 \
        -DNACL_LINUX=1 \
        -ffixed-r9 \
        -isystem $(SDK_ROOT)/arm-newlib/newlib_extra_header \
        -isystem $(SDK_ROOT)/codesourcery/arm-2007q3/arm-none-linux-gnueabi/include/c++/4.2.1 \
        -isystem $(SDK_ROOT)/arm-newlib/arm-none-linux-gnueabi/include \
        -isystem $(SDK_ROOT)/arm-newlib/arm-none-linux-gnueabi/include/sys


NACL_LINK=$(SDK_ROOT)/arm-none-linux-gnueabi/llvm-fake-sfigcc \
          -static \
          -nostdlib \
          -Wl,-T -Wl,$(SDK_ROOT)/arm-none-linux-gnueabi/ld_script_arm_untrusted \
          -L$(SDK_ROOT)/arm-newlib/arm-none-linux-gnueabi/lib \
          -L$(SDK_ROOT)/arm-newlib/arm-none-linux-gnueabi/usr/lib

NACL_LINK_FIRST=$(SDK_ROOT)/arm-newlib/arm-none-linux-gnueabi/lib/crt1.o \
                $(SDK_ROOT)/arm-newlib/arm-none-linux-gnueabi/lib/crti.o

# NOTE: replication of -lc is intentional
NACL_LINK_LAST=-lc -lnacl -lc -lgcc -lunimpl \
               $(SDK_ROOT)/arm-newlib/arm-none-linux-gnueabi/lib/crtn.o

.SUFFIXES: .o .c

.c.o:
	${NACL_CC} -o $@ -c $<


# ======================================================================
#
# ======================================================================
help:
	@echo "usage"
	@echo ""
	@echo "make hello.nexe : build the hello world nacl module"
	@echo ""
	@echo "make hello_run : run the hello world nacl module"
	@echo ""
	@echo "make hello_validate : validate on the hello world nacl module"
	@echo ""
	@echo "make clean : remove temporary files"


clean:
	rm -f *.o *.s *.bc *.nexe


hello.nexe: hello.o
	$(NACL_LINK) $(NACL_LINK_FIRST)  $< $(NACL_LINK_LAST) -o $@


# -d disables validation
# -f permits file access
hello_run: hello.nexe
	$(QEMU_SCRIPT) run $(SEL_LDR) -d -f $<


hello_validate: hello.nexe
	/usr/local/crosstool/tools-x86/arm-ncval-core $<

