# Copyright 2008 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

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

SEL_LDR_ARM = 'scons-out/opt-linux-arm/staging/sel_ldr'

SEL_LDR_X32 = 'scons-out/opt-linux-x86-32/staging/sel_ldr'

SEL_LDR_X64 = 'scons-out/opt-linux-x86-64/staging/sel_ldr'

NACL_GCC_X32 = 'toolchain/linux_x86/bin/nacl-gcc'

NACL_GCC_X64 = 'toolchain/linux_x86/bin/nacl64-gcc'

GLOBAL_CFLAGS = '-DSTACK_SIZE=0x40000 -DNO_TRAMPOLINES -DNO_LABEL_VALUES'
######################################################################
# LOCAL GCC
######################################################################
COMMANDS_local_gcc = [
    ('compile',
     '%(CC)s %(src)s %(CFLAGS)s -o %(tmp)s.exe',
     ),
    ]

TOOLCHAIN_CONFIGS['local_gcc_x8632_O0'] = ToolchainConfig(
    desc='local gcc [x86-32]',
    commands=COMMANDS_local_gcc,
    tools_needed=[LOCAL_GCC],
    CC = LOCAL_GCC,
    CFLAGS = '-O0 -m32 -static ' + GLOBAL_CFLAGS)

TOOLCHAIN_CONFIGS['local_gcc_x8664_O0'] = ToolchainConfig(
    desc='local gcc [x86-64]',
    commands=COMMANDS_local_gcc,
    tools_needed=[LOCAL_GCC],
    CC = LOCAL_GCC,
    CFLAGS = '-O0 -m64 -static ' + GLOBAL_CFLAGS)

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

TOOLCHAIN_CONFIGS['gcc_cs_arm_O9'] = ToolchainConfig(
    desc='codesourcery cross gcc [arm]',
    commands=COMMANDS_gcc_cs_arm,
    tools_needed=[GCC_CS_ARM, EMU_SCRIPT ],
    CC = GCC_CS_ARM,
    EMU_SCRIPT = EMU_SCRIPT,
    CFLAGS = '-O9 -static ' + GLOBAL_CFLAGS)

######################################################################
# # NACL + SEL_LDR [X86]
######################################################################
COMMANDS_nacl_gcc = [
    ('compile',
     '%(CC)s %(src)s %(CFLAGS)s -o %(tmp)s.exe',
     ),
    ('sel_ldr',
     '%(SEL_LDR)s -f %(tmp)s.exe',
     )
  ]


TOOLCHAIN_CONFIGS['nacl_gcc_x8632_O0'] = ToolchainConfig(
    desc='nacl gcc [x86-32]',
    commands=COMMANDS_nacl_gcc,
    tools_needed=[NACL_GCC_X32, SEL_LDR_X32],
    CC = NACL_GCC_X32,
    SEL_LDR = SEL_LDR_X32,
    CFLAGS = '-O0 -static ' + GLOBAL_CFLAGS)

TOOLCHAIN_CONFIGS['nacl_gcc_x8632_O9'] = ToolchainConfig(
    desc='nacl gcc with optimizations [x86-32]',
    commands=COMMANDS_nacl_gcc,
    tools_needed=[NACL_GCC_X32, SEL_LDR_X32],
    CC = NACL_GCC_X32,
    SEL_LDR = SEL_LDR_X32,
    CFLAGS = '-O9 -static')

TOOLCHAIN_CONFIGS['nacl_gcc_x8664_O0'] = ToolchainConfig(
    desc='nacl gcc [x86-64]',
    commands=COMMANDS_nacl_gcc,
    tools_needed=[NACL_GCC_X64, SEL_LDR_X64],
    CC = NACL_GCC_X64,
    SEL_LDR = SEL_LDR_X64,
    CFLAGS = '-O0 -static ' + GLOBAL_CFLAGS)

TOOLCHAIN_CONFIGS['nacl_gcc_x8664_O9'] = ToolchainConfig(
    desc='nacl gcc with optimizations [x86-64]',
    commands=COMMANDS_nacl_gcc,
    tools_needed=[NACL_GCC_X64, SEL_LDR_X64],
    CC = NACL_GCC_X64,
    SEL_LDR = SEL_LDR_X64,
    CFLAGS = '-O9 -static ' + GLOBAL_CFLAGS)


######################################################################
# PNACL + SEL_LDR [ARM]
######################################################################
DRIVER_PATH = 'toolchain/linux_arm-untrusted/bin'

PNACL_LLVM_GCC = DRIVER_PATH + '/pnacl-gcc'

PNACL_BCLD = DRIVER_PATH + '/pnacl-bcld'

PNACL_LIB_DIR = 'toolchain/linux_arm-untrusted/libs-bitcode/'

COMMANDS_llvm_pnacl_arm = [
    ('compile-bc',
     '%(CC)s %(src)s %(CFLAGS)s -c -o %(tmp)s.bc',
     ),
    ('translate-arm',
     '%(LD)s %(tmp)s.bc -o %(tmp)s.nexe  -L%(LIB_DIR)s -lc -lnacl -lnosys',
     ),
    ('qemu-sel_ldr',
     '%(EMU)s run %(SEL_LDR)s -Q %(tmp)s.nexe',
     )
  ]


TOOLCHAIN_CONFIGS['llvm_pnacl_arm_O0'] = ToolchainConfig(
    desc='pnacl llvm [arm]',
    commands=COMMANDS_llvm_pnacl_arm,
    tools_needed=[PNACL_LLVM_GCC, PNACL_BCLD, EMU_SCRIPT, SEL_LDR_ARM],
    CC = PNACL_LLVM_GCC + ' -emit-llvm',
    LD = PNACL_BCLD + ' -arch arm',
    EMU = EMU_SCRIPT,
    SEL_LDR = SEL_LDR_ARM,
    LIB_DIR = PNACL_LIB_DIR,
    CFLAGS = '-O0 -static ' + GLOBAL_CFLAGS)


TOOLCHAIN_CONFIGS['llvm_pnacl_arm_O9'] = ToolchainConfig(
    desc='pnacl llvm with optimizations [arm]',
    commands=COMMANDS_llvm_pnacl_arm,
    tools_needed=[PNACL_LLVM_GCC, PNACL_BCLD, EMU_SCRIPT, SEL_LDR_ARM],
    CC = PNACL_LLVM_GCC + ' -emit-llvm',
    LD = PNACL_BCLD  + ' -arch arm',
    EMU = EMU_SCRIPT,
    SEL_LDR = SEL_LDR_ARM,
    LIB_DIR = PNACL_LIB_DIR,
    CFLAGS = '-09 -static ' + GLOBAL_CFLAGS)

######################################################################
# PNACL + SEL_LDR [X8632]
######################################################################

# NOTE: this is used for both x86 flavors
COMMANDS_llvm_pnacl_x86_O0 = [
    ('compile-bc',
     '%(CC)s %(src)s %(CFLAGS)s -c -o %(tmp)s.bc',
     ),
    ('translate-x8632',
     '%(LD)s %(tmp)s.bc -o %(tmp)s.nexe  -L%(LIB_DIR)s',
     ),
    ('sel_ldr',
     '%(SEL_LDR)s %(tmp)s.nexe',
     )
  ]


TOOLCHAIN_CONFIGS['llvm_pnacl_x8632_O0'] = ToolchainConfig(
    desc='pnacl llvm [x8632]',
    commands=COMMANDS_llvm_pnacl_x86_O0,
    tools_needed=[PNACL_LLVM_GCC, PNACL_BCLD, SEL_LDR_X32],
    CC = PNACL_LLVM_GCC + ' -emit-llvm',
    LD = PNACL_BCLD + ' -arch x86-32',
    SEL_LDR = SEL_LDR_X32,
    LIB_DIR = PNACL_LIB_DIR,
    CFLAGS = '-O0 -static ' + GLOBAL_CFLAGS)

######################################################################
# PNACL + SEL_LDR [X8664]
######################################################################


TOOLCHAIN_CONFIGS['llvm_pnacl_x8664_O0'] = ToolchainConfig(
    desc='pnacl llvm [x8664]',
    commands=COMMANDS_llvm_pnacl_x86_O0,
    tools_needed=[PNACL_LLVM_GCC, PNACL_BCLD, SEL_LDR_X64],
    CC = PNACL_LLVM_GCC + ' -emit-llvm',
    LD = PNACL_BCLD + ' -arch x86-64',
    SEL_LDR = SEL_LDR_X64,
    LIB_DIR = PNACL_LIB_DIR,
    CFLAGS = '-O0 -static ' + GLOBAL_CFLAGS)
