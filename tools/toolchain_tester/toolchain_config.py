#
# Config using the llvm for arm
#
import os


TOOLCHAIN_CONFIGS = {}



class ToolchainConfig(object):
  def __init__(self, desc, commands, tools_needed, **extra):
    self._desc = desc,
    self._commands = commands
    self._tools_needed = tools_needed
    self._extra = extra

  def SanityCheck(self):
    for t in self._tools_needed:
      assert os.access(t, os.R_OK | os.X_OK)

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
LLVM_GCC = 'toolchain/llvm-cs/arm-none-linux-gnueabi/llvm-gcc-4.2//bin/llvm-gcc'
EMU_SCRIPT = 'toolchain/linux_arm-trusted/qemu_tool.sh'

SEL_LDR_X32 = 'scons-out/opt-linux-x86-32/staging/sel_ldr'

SEL_LDR_X64 = 'scons-out/opt-linux-x86-64/staging/sel_ldr'

NACL_GCC_X32 = 'toolchain/linux_x86/sdk/nacl-sdk/bin/nacl-gcc'

NACL_GCC_X64 = 'toolchain/linux_x86/sdk/nacl-sdk/bin/nacl64-gcc'

GLOBAL_CFLAGS = '-DSTACK_SIZE=0x40000 -DNO_TRAMPOLINES -DNO_LABEL_VALUES'
######################################################################
# LOCAL GCC
######################################################################
COMMANDS = [
    ('compile',
     '%(CC)s %(src)s %(CFLAGS)s -o %(tmp)s.exe',
     ),
    ]

TOOLCHAIN_CONFIGS['local_gcc_x32_O0'] = ToolchainConfig(
    '',
    COMMANDS,
    [LOCAL_GCC],
    CC = LOCAL_GCC,
    CFLAGS = '-O0 -m32 -static ' + GLOBAL_CFLAGS)

######################################################################
# CS ARM
######################################################################

GCC_CS_ARM = 'toolchain/linux_arm-trusted/arm-2009q3/bin/arm-none-linux-gnueabi-gcc'

COMMANDS = [
    ('compile',
     '%(CC)s %(src)s %(CFLAGS)s -o %(tmp)s.exe',
     ),
    ('emu',
     '%(EMU_SCRIPT)s run %(tmp)s.exe',
     )
    ]

TOOLCHAIN_CONFIGS['gcc_cs_arm_O0'] = ToolchainConfig(
    '',
    COMMANDS,
    [GCC_CS_ARM, EMU_SCRIPT ],
    CC = GCC_CS_ARM,
    EMU_SCRIPT = EMU_SCRIPT,
    CFLAGS = '-O0 -static ' + GLOBAL_CFLAGS)


######################################################################
# NACL_ARM + SEL_LDR
######################################################################
COMMANDS = [
    ('compile',
     '%(LLVM_GCC)s %(src)s %(CFLAGS)s -o %(tmp)s.exe',
     ),
    ('emu',
     '%(EMU_SCRIPT)s run %(tmp)s.exe',
     )
  ]


TOOLCHAIN_CONFIGS['llvm_cs_arm_O0'] = ToolchainConfig(
    '',
    COMMANDS,
    [LLVM_GCC, EMU_SCRIPT],
    LLVM_GCC = LLVM_GCC,
    EMU_SCRIPT = EMU_SCRIPT,
    CFLAGS = '-O0 -static ' + GLOBAL_CFLAGS)

######################################################################
# # NACL_X86 + SEL_LDR
######################################################################
COMMANDS = [
    ('compile',
     '%(NACL_GCC)s %(src)s %(CFLAGS)s -o %(tmp)s.exe',
     ),
    ('sel_ldr',
     '%(SEL_LDR)s -f %(tmp)s.exe',
     )
  ]


TOOLCHAIN_CONFIGS['nacl_gcc32_O0'] = ToolchainConfig(
    '',
    COMMANDS,
    [NACL_GCC_X32, SEL_LDR_X32],
    NACL_GCC = NACL_GCC_X32,
    SEL_LDR = SEL_LDR_X32,
    CFLAGS = '-O0 -static ' + GLOBAL_CFLAGS)

TOOLCHAIN_CONFIGS['nacl_gcc32_O9'] = ToolchainConfig(
    '',
    COMMANDS,
    [NACL_GCC_X32, SEL_LDR_X32],
    NACL_GCC = NACL_GCC_X32,
    SEL_LDR = SEL_LDR_X32,
    CFLAGS = '-O9 -static')


TOOLCHAIN_CONFIGS['nacl_gcc64_O0'] = ToolchainConfig(
    '',
    COMMANDS,
    [NACL_GCC_X64, SEL_LDR_X64],
    NACL_GCC = NACL_GCC_X64,
    SEL_LDR = SEL_LDR_X64,
    CFLAGS = '-O0 -static ' + GLOBAL_CFLAGS)

TOOLCHAIN_CONFIGS['nacl_gcc64_O9'] = ToolchainConfig(
    '',
    COMMANDS,
    [NACL_GCC_X64, SEL_LDR_X64],
    NACL_GCC = NACL_GCC_X64,
    SEL_LDR = SEL_LDR_X64,
    CFLAGS = '-O9 -static ' + GLOBAL_CFLAGS)


######################################################################
# PNACL + SEL_LDR [ARM]
######################################################################

DRIVER_PATH = 'toolchain/linux_arm-untrusted/arm-none-linux-gnueabi'

PNACL_LLVM_GCC = DRIVER_PATH + '/llvm-fake-bcgcc'

NACL_LLVM_GCC_ARM = DRIVER_PATH + '/llvm-fake-sfigcc'

NACL_LD_ARM = DRIVER_PATH + '/llvm-fake-sfild'

PNACL_BCLD_ARM = DRIVER_PATH + '/llvm-fake-bcld'

SEL_LDR_ARM = 'scons-out/opt-linux-arm/staging/sel_ldr'

LIB_DIR = 'toolchain/linux_arm-untrusted/arm-newlib/arm-none-linux-gnueabi/lib'

COMMANDS = [
    ('compile-o',
     '%(NACL_GCC)s %(src)s %(CFLAGS)s -c -o %(tmp)s.o',
     ),
    ('ld-arm',
     '%(NACL_LD)s %(tmp)s.o -L %(LIB_DIR)s -lc -lnacl -o %(tmp)s.nexe',
     ),
    ('qemu-sel_ldr',
     '%(EMU_SCRIPT)s run %(SEL_LDR_ARM)s -Q -f %(tmp)s.nexe',
     )
  ]


TOOLCHAIN_CONFIGS['llvm_nacl_sfi_arm_O0'] = ToolchainConfig(
    '',
    COMMANDS,
    [NACL_LLVM_GCC_ARM, NACL_LD_ARM, SEL_LDR_ARM, EMU_SCRIPT],
    NACL_GCC = NACL_LLVM_GCC_ARM,
    NACL_LD = NACL_LD_ARM,
    EMU_SCRIPT = EMU_SCRIPT,
    LIB_DIR = LIB_DIR,
    SEL_LDR_ARM = SEL_LDR_ARM,
    CFLAGS = '-O0  -fnested-functions ' + GLOBAL_CFLAGS)


COMMANDS = [
    ('compile-bc',
     '%(PNACL_LLVM_GCC)s %(src)s %(CFLAGS)s -o %(tmp)s.bc',
     ),
    ('translate-arm',
     '%(PNACL_BCLD)s %(tmp)s.bc -o %(tmp)s.nexe',
     ),
    ('qemu-sel_ldr',
     '%(EMU_SCRIPT)s run %(SEL_LDR_ARM)s %(tmp)s.nexe',
     )
  ]


TOOLCHAIN_CONFIGS['llvm_pnacl_arm_O0'] = ToolchainConfig(
    '',
    COMMANDS,
    [PNACL_LLVM_GCC, PNACL_BCLD_ARM, EMU_SCRIPT],
    PNACL_LLVM_GCC = PNACL_LLVM_GCC,
    PNACL_BCLD = PNACL_BCLD_ARM,
    EMU_SCRIPT = EMU_SCRIPT,
    CFLAGS = '-O0 -static ' + GLOBAL_CFLAGS)
