# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# Config file for various nacl compilation scenarios
#
import os
import sys

TOOLCHAIN_CONFIGS = {}


class ToolchainConfig(object):
  def __init__(self, desc, commands, tools_needed, **extra):
    self._desc = desc,
    self._commands = commands
    self._tools_needed = tools_needed
    self._extra = extra

  def Append(self, tag, value):
    assert tag in self._extra
    self._extra[tag] = self._extra[tag] + ' ' + value + ' '

  def SanityCheck(self):
    for t in self._tools_needed:
      if not os.access(t, os.R_OK | os.X_OK):
        print "ERROR: missing tool ", t
        sys.exit(-1)

  def GetDescription(self):
    return self._desc

  def GetCommands(self, extra):
    for tag, val in self._commands:
      d = {}
      d.update(self._extra)
      d.update(extra)
      yield tag, val % d

  def GetPhases(self):
    return [a for (a, _) in self._commands]


######################################################################
#
######################################################################

LOCAL_GCC = '/usr/bin/gcc'

EMU_SCRIPT = 'toolchain/linux_arm-trusted/qemu_tool.sh'

BOOTSTRAP_ARM = 'scons-out/opt-linux-arm/staging/nacl_helper_bootstrap'
SEL_LDR_ARM = BOOTSTRAP_ARM + ' scons-out/opt-linux-arm/staging/sel_ldr'
IRT_ARM = 'scons-out/nacl_irt-arm/obj/src/untrusted/irt/irt_core.nexe'

BOOTSTRAP_X32 = 'scons-out/opt-linux-x86-32/staging/nacl_helper_bootstrap'
SEL_LDR_X32 = BOOTSTRAP_X32 + ' scons-out/opt-linux-x86-32/staging/sel_ldr'
IRT_X32 = 'scons-out/nacl_irt-x86-32/obj/src/untrusted/irt/irt_core.nexe'

BOOTSTRAP_X64 = 'scons-out/opt-linux-x86-64/staging/nacl_helper_bootstrap'
SEL_LDR_X64 = BOOTSTRAP_X64 + ' scons-out/opt-linux-x86-64/staging/sel_ldr'
IRT_X64 = 'scons-out/nacl_irt-x86-64/obj/src/untrusted/irt/irt_core.nexe'

NACL_GCC_X32 = 'toolchain/linux_x86_newlib/bin/i686-nacl-gcc'

NACL_GCC_X64 = 'toolchain/linux_x86_newlib/bin/x86_64-nacl-gcc'

GLOBAL_CFLAGS = ' '.join(['-DSTACK_SIZE=0x40000',
                          '-D__SIZEOF_INT__=4',
                          '-D__SIZEOF_LONG__=4',
                          '"-D__INT_LEAST8_TYPE__=signed char"',
                          '"-D__UINT_LEAST32_TYPE__=unsigned int"',
                          '-D_XOPEN_SOURCE=600',
                          '-DNO_TRAMPOLINES',
                          '-DNO_LABEL_VALUES',])

CLANG_CFLAGS = ' '.join(['-fwrapv',
                         '-fdiagnostics-show-category=name'])

######################################################################
# LOCAL GCC
######################################################################
COMMANDS_local_gcc = [
    ('compile',
     '%(CC)s %(src)s %(CFLAGS)s -o %(tmp)s.exe -lm -lstdc++',
     ),
    ('run',
     '%(tmp)s.exe',
     ),
    ]

TOOLCHAIN_CONFIGS['local_gcc_x8632_O0'] = ToolchainConfig(
    desc='local gcc [x86-32]',
    commands=COMMANDS_local_gcc,
    tools_needed=[LOCAL_GCC],
    CC = LOCAL_GCC,
    CFLAGS = '-O0 -m32 -static ' + GLOBAL_CFLAGS)

TOOLCHAIN_CONFIGS['local_gcc_x8632_O3'] = ToolchainConfig(
    desc='local gcc [x86-32]',
    commands=COMMANDS_local_gcc,
    tools_needed=[LOCAL_GCC],
    CC = LOCAL_GCC,
    CFLAGS = '-O3 -m32 -static ' + GLOBAL_CFLAGS)

TOOLCHAIN_CONFIGS['local_gcc_x8664_O0'] = ToolchainConfig(
    desc='local gcc [x86-64]',
    commands=COMMANDS_local_gcc,
    tools_needed=[LOCAL_GCC],
    CC = LOCAL_GCC,
    CFLAGS = '-O0 -m64 -static ' + GLOBAL_CFLAGS)

TOOLCHAIN_CONFIGS['local_gcc_x8664_O3'] = ToolchainConfig(
    desc='local gcc [x86-64]',
    commands=COMMANDS_local_gcc,
    tools_needed=[LOCAL_GCC],
    CC = LOCAL_GCC,
    CFLAGS = '-O3 -m64 -static ' + GLOBAL_CFLAGS)

######################################################################
# CS ARM
######################################################################
# NOTE: you may need this if you see mmap: Permission denied
# "echo 0 > /proc/sys/vm/mmap_min_addr"
GCC_CS_ARM = ('toolchain/linux_arm-trusted/arm-2009q3/' +
              'bin/arm-none-linux-gnueabi-gcc')

COMMANDS_gcc_cs_arm = [
    ('compile',
     '%(CC)s %(src)s %(CFLAGS)s -Wl,-Ttext-segment=20000  -o %(tmp)s.exe',
     ),
    ('emu',
     '%(EMU_SCRIPT)s run %(tmp)s.exe',
     )
    ]

TOOLCHAIN_CONFIGS['gcc_cs_arm_O0'] = ToolchainConfig(
    desc='codesourcery cross gcc [arm]',
    commands=COMMANDS_gcc_cs_arm,
    tools_needed=[GCC_CS_ARM, EMU_SCRIPT ],
    CC = GCC_CS_ARM,
    EMU_SCRIPT = EMU_SCRIPT,
    CFLAGS = '-O0 -static ' + GLOBAL_CFLAGS)

TOOLCHAIN_CONFIGS['gcc_cs_arm_O3'] = ToolchainConfig(
    desc='codesourcery cross gcc [arm]',
    commands=COMMANDS_gcc_cs_arm,
    tools_needed=[GCC_CS_ARM, EMU_SCRIPT ],
    CC = GCC_CS_ARM,
    EMU_SCRIPT = EMU_SCRIPT,
    CFLAGS = '-O3 -static ' + GLOBAL_CFLAGS)

######################################################################
# # NACL + SEL_LDR [X86]
######################################################################
COMMANDS_nacl_gcc = [
    ('compile',
     '%(CC)s %(src)s %(CFLAGS)s -o %(tmp)s.exe -lm -lstdc++',
     ),
    ('sel_ldr',
     '%(SEL_LDR)s -B %(IRT)s %(tmp)s.exe',
     )
  ]


TOOLCHAIN_CONFIGS['nacl_gcc_x8632_O0'] = ToolchainConfig(
    desc='nacl gcc [x86-32]',
    commands=COMMANDS_nacl_gcc,
    tools_needed=[NACL_GCC_X32, SEL_LDR_X32],
    CC = NACL_GCC_X32,
    SEL_LDR = SEL_LDR_X32,
    IRT = IRT_X32,
    CFLAGS = '-O0 -static -Bscons-out/nacl-x86-32/lib/ ' + GLOBAL_CFLAGS)

TOOLCHAIN_CONFIGS['nacl_gcc_x8632_O3'] = ToolchainConfig(
    desc='nacl gcc with optimizations [x86-32]',
    commands=COMMANDS_nacl_gcc,
    tools_needed=[NACL_GCC_X32, SEL_LDR_X32],
    CC = NACL_GCC_X32,
    SEL_LDR = SEL_LDR_X32,
    IRT = IRT_X32,
    CFLAGS = '-O3 -static -Bscons-out/nacl-x86-32/lib/ ' + GLOBAL_CFLAGS)

TOOLCHAIN_CONFIGS['nacl_gcc_x8664_O0'] = ToolchainConfig(
    desc='nacl gcc [x86-64]',
    commands=COMMANDS_nacl_gcc,
    tools_needed=[NACL_GCC_X64, SEL_LDR_X64],
    CC = NACL_GCC_X64,
    SEL_LDR = SEL_LDR_X64,
    IRT = IRT_X64,
    CFLAGS = '-O0 -static -Bscons-out/nacl-x86-64/lib/ ' + GLOBAL_CFLAGS)

TOOLCHAIN_CONFIGS['nacl_gcc_x8664_O3'] = ToolchainConfig(
    desc='nacl gcc with optimizations [x86-64]',
    commands=COMMANDS_nacl_gcc,
    tools_needed=[NACL_GCC_X64, SEL_LDR_X64],
    CC = NACL_GCC_X64,
    SEL_LDR = SEL_LDR_X64,
    IRT = IRT_X64,
    CFLAGS = '-O3 -static -Bscons-out/nacl-x86-64/lib/ ' + GLOBAL_CFLAGS)

######################################################################
# PNACL + SEL_LDR [ARM]
######################################################################
PNACL_ROOT = 'toolchain/pnacl_linux_x86_64_newlib'

PNACL_FRONTEND = PNACL_ROOT + '/bin/pnacl-clang'

# NOTE: Our driver supports going from .c to .nexe in one go
#       but it maybe useful to inspect the bitcode file so we
#       split the compilation into two steps.
PNACL_LD = PNACL_ROOT + '/bin/pnacl-clang'

COMMANDS_llvm_pnacl_arm = [
    ('compile-bc',
     '%(CC)s %(src)s %(CFLAGS)s -c -o %(tmp)s.bc',
     ),
    ('translate-arm',
     '%(LD)s %(tmp)s.bc -lm -lstdc++ -o %(tmp)s.nexe',
     ),
    ('qemu-sel_ldr',
     '%(EMU)s run %(SEL_LDR)s -B %(IRT)s -Q %(tmp)s.nexe',
     )
  ]


TOOLCHAIN_CONFIGS['llvm_pnacl_arm_O0'] = ToolchainConfig(
    desc='pnacl llvm [arm]',
    commands=COMMANDS_llvm_pnacl_arm,
    tools_needed=[PNACL_FRONTEND, PNACL_LD, EMU_SCRIPT, SEL_LDR_ARM],
    CC = PNACL_FRONTEND,
    LD = PNACL_LD + ' -arch arm',
    EMU = EMU_SCRIPT,
    SEL_LDR = SEL_LDR_ARM,
    IRT = IRT_ARM,
    CFLAGS = '-O0 -static ' + CLANG_CFLAGS + ' ' + GLOBAL_CFLAGS)


TOOLCHAIN_CONFIGS['llvm_pnacl_arm_O3'] = ToolchainConfig(
    desc='pnacl llvm with optimizations [arm]',
    commands=COMMANDS_llvm_pnacl_arm,
    tools_needed=[PNACL_FRONTEND, PNACL_LD, EMU_SCRIPT, SEL_LDR_ARM],
    CC = PNACL_FRONTEND,
    LD = PNACL_LD  + ' -arch arm',
    EMU = EMU_SCRIPT,
    SEL_LDR = SEL_LDR_ARM,
    IRT = IRT_ARM,
    CFLAGS = '-O3 -D__OPTIMIZE__ -static ' + CLANG_CFLAGS  + ' '
              + GLOBAL_CFLAGS)

######################################################################
# PNACL + SEL_LDR [X8632]
######################################################################

# NOTE: this is used for both x86 flavors
COMMANDS_llvm_pnacl_x86_O0 = [
    ('compile-bc',
     '%(CC)s %(src)s %(CFLAGS)s -c -o %(tmp)s.bc',
     ),
    ('translate-x86',
     '%(LD)s %(tmp)s.bc -lm -lstdc++ -o %(tmp)s.nexe ',
     ),
    ('sel_ldr',
     '%(SEL_LDR)s -B %(IRT)s %(tmp)s.nexe',
     )
  ]


TOOLCHAIN_CONFIGS['llvm_pnacl_x8632_O0'] = ToolchainConfig(
    desc='pnacl llvm [x8632]',
    commands=COMMANDS_llvm_pnacl_x86_O0,
    tools_needed=[PNACL_FRONTEND, PNACL_LD, SEL_LDR_X32],
    CC = PNACL_FRONTEND,
    LD = PNACL_LD + ' -arch x86-32',
    SEL_LDR = SEL_LDR_X32,
    IRT = IRT_X32,
    CFLAGS = '-O0  -static ' + CLANG_CFLAGS + ' ' + GLOBAL_CFLAGS)

TOOLCHAIN_CONFIGS['llvm_pnacl_x8632_O3'] = ToolchainConfig(
    desc='pnacl llvm [x8632]',
    commands=COMMANDS_llvm_pnacl_x86_O0,
    tools_needed=[PNACL_FRONTEND, PNACL_LD, SEL_LDR_X32],
    CC = PNACL_FRONTEND,
    LD = PNACL_LD + ' -arch x86-32',
    SEL_LDR = SEL_LDR_X32,
    IRT = IRT_X32,
    CFLAGS = '-O3 -D__OPTIMIZE__ -static ' + CLANG_CFLAGS + ' '
             + GLOBAL_CFLAGS)

######################################################################
# PNACL + SEL_LDR [X8664]
######################################################################

TOOLCHAIN_CONFIGS['llvm_pnacl_x8664_O0'] = ToolchainConfig(
    desc='pnacl llvm [x8664]',
    commands=COMMANDS_llvm_pnacl_x86_O0,
    tools_needed=[PNACL_FRONTEND, PNACL_LD, SEL_LDR_X64],
    CC = PNACL_FRONTEND,
    LD = PNACL_LD + ' -arch x86-64',
    SEL_LDR = SEL_LDR_X64,
    IRT = IRT_X64,
    CFLAGS = '-O0 -static ' + CLANG_CFLAGS + ' ' + GLOBAL_CFLAGS)

TOOLCHAIN_CONFIGS['llvm_pnacl_x8664_O3'] = ToolchainConfig(
    desc='pnacl llvm [x8664]',
    commands=COMMANDS_llvm_pnacl_x86_O0,
    tools_needed=[PNACL_FRONTEND, PNACL_LD, SEL_LDR_X64],
    CC = PNACL_FRONTEND,
    LD = PNACL_LD + ' -arch x86-64',
    SEL_LDR = SEL_LDR_X64,
    IRT = IRT_X64,
    CFLAGS = '-O3 -D__OPTIMIZE__ -static ' + CLANG_CFLAGS + ' '
             + GLOBAL_CFLAGS)
