#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is a replacement for llvm-gcc and llvm-g++ driver.
# It detects automatically which role it is supposed to assume.
# The point of the script is to redirect builds through our own tools,
# while making these tools appear like gnu tools.
#
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  tools/llvm/utman.sh driver
# in order for them to take effect in the scons build.  This command
# updates the copy in the toolchain/ tree.
#
# The code is broken down into sections:
#   1) Environment Settings
#   2) Patterns for command-line arguments
#   3) Chain Map
#   4) Main and Incarnations
#   5) Compiling / Linking Engine
#   6) Chain Logic
#   7) Environment
#   8) Argument Parsing
#   9) File Naming Logic
#   10) Shell Utilities
#   11) Logging
#   12) TLS Hack
#   13) Invocation

import os
import re
import shutil
import subprocess
import sys
import signal
import platform

######################################################################
# Environment Settings
######################################################################

# This dictionary initializes a shell-like environment.
# Shell escaping and ${} substitution are provided.
# See "class env" defined later for the implementation.

INITIAL_ENV = {
  # Internal settings
  'EMIT_LL'     : '1',    # Produce an intermediate .ll file
                          # Currently enabled for debugging.
                          # TODO(pdox): Disable for SDK version
  'USE_MC_ASM'  : '1',    # Use llvm-mc instead of nacl-as
  'SINGLE_BC_LIBS': '0',  # 'archive' bitcode libs as single bitcode file
  'MC_DIRECT'   : '1',    # Use MC direct object emission instead of
                          # producing an intermediate .s file
  'WHICH_LD'    : 'BFD',  # Which ld to use for native linking: GOLD or BFD
  'CLEANUP'     : '0',    # Clean up temporary files
                          # TODO(pdox): Enable for SDK version
  'SANDBOXED'   : '0',    # Use sandboxed toolchain for this arch. (main switch)
  'SANDBOXED_LLC': '0',   # Use sandboxed LLC
  'SANDBOXED_AS': '0',    # Use sandboxed AS
  'SANDBOXED_LD': '0',    # Use sandboxed LD
  'SRPC'        : '1',    # Use SRPC sandboxed toolchain
  'GOLD_FIX'    : '0',    # Use linker script instead of -Ttext for gold.
                          # Needed for dynamic_code_loading tests which create
                          # a gap between text and rodata.
                          # TODO(pdox): Either eliminate gold native linking or
                          #             figure out why this is broken in the
                          #             first place.
  'BIAS'        : 'NONE', # This can be 'NONE', 'ARM', 'X8632', or 'X8664'.
                          # When not set to none, this causes the front-end to
                          # act like a target-specific compiler. This bias is
                          # currently needed while compiling llvm-gcc, newlib,
                          # and some scons tests.
                          # TODO(pdox): Can this be removed?
  # Command-line options
  'GCC_MODE'    : '',     # '' (default), '-E', '-c', or '-S'
  'PIC'         : '0',    # -fPIC
  'STATIC'      : '1',    # -static was given (on by default for now)
  'SHARED'      : '0',    # Produce a shared library
  'NOSTDINC'    : '0',    # -nostdinc
  'NOSTDLIB'    : '0',    # -nostdlib
  'DIAGNOSTIC'  : '0',    # Diagnostic flag detected
  'SEARCH_DIRS' : '',     # Library search directories

  'INPUTS'      : '',    # Input files
  'OUTPUT'      : '',    # Output file
  'UNMATCHED'   : '',    # All unrecognized parameters
  'ARGV'        : '',    # argv of the incarnation

  # Special settings needed to tweak behavior
  'SKIP_OPT'    : '0',  # Don't run OPT. This is used in cases where
                        # OPT might break things.

  # These are filled in by env.reset()
  # TODO(pdox): It should not be necessary to auto-detect these here. Change
  #             detection method so that toolchain location is relative.
  'BASE_NACL'       : '',   # Absolute path of native_client/ dir
  'BUILD_OS'        : '',   # "linux" or "darwin"
  'BUILD_ARCH'      : '',   # "x86_64" or "i686" or "i386"

  'BASE_TOOLCHAIN'  : '${BASE_NACL}/toolchain',
  'BASE'            : '${BASE_TOOLCHAIN}/pnacl_${BUILD_OS}_${BUILD_ARCH}',
  'BASE_TRUSTED'    : '${BASE_TOOLCHAIN}/linux_arm-trusted',
  'BASE_ARM'        : '${BASE}/arm-none-linux-gnueabi',
  'BASE_ARM_INCLUDE': '${BASE_ARM}/arm-none-linux-gnueabi/include',
  'BASE_BIN'        : '${BASE}/bin',

  'DRY_RUN'              : '0',

  'LOG_TO_FILE'          : '1',
  'LOG_FILENAME'         : '${BASE_TOOLCHAIN}/hg-log/driver.log',
  'LOG_FILE_SIZE_LIMIT'  : str(20 * 1024 * 1024),
  'LOG_PRETTY_PRINT'     : '1',

  'PNACL_AS_ARM'         : '${BASE_BIN}/pnacl-arm-as',
  'PNACL_AS_X8632'       : '${BASE_BIN}/pnacl-i686-as',
  'PNACL_AS_X8664'       : '${BASE_BIN}/pnacl-x86_64-as',

  'SO_EXT'          : '${SO_EXT_%BUILD_OS%}',
  'SO_EXT_darwin'   : '.dylib',
  'SO_EXT_linux'    : '.so',
  'GOLD_PLUGIN_SO'  : '${BASE_ARM}/lib/libLLVMgold${SO_EXT}',
  'GOLD_PLUGIN_ARGS': '-plugin=${GOLD_PLUGIN_SO} ' +
                      '-plugin-opt=emit-llvm',

  'ROOT_ARM'    : '${BASE}/libs-arm',
  'ROOT_X8632'  : '${BASE}/libs-x8632',
  'ROOT_X8664'  : '${BASE}/libs-x8664',
  'ROOT_BC'     : '${BASE}/libs-bitcode',

  'TRIPLE_ARM'  : 'armv7a-none-linux-gnueabi',
  'TRIPLE_X8632': 'i686-none-linux-gnu',
  'TRIPLE_X8664': 'x86_64-none-linux-gnu',

  'LLVM_GCC'    : '${BASE_ARM}/bin/arm-none-linux-gnueabi-gcc',
  'LLVM_GXX'    : '${BASE_ARM}/bin/arm-none-linux-gnueabi-g++',

  # We need to remove the pre-existing ARM defines which appear
  # because we are using the ARM llvm-gcc frontend.
  # TODO(pdox): Prevent these from being defined by llvm-gcc in
  #             the first place by creating a custom target for PNaCl.
  'REMOVE_BIAS' : '-U__arm__ -U__APCS_32__ -U__thumb__ -U__thumb2__ ' +
                  '-U__ARMEB__ -U__THUMBEB__ -U__ARMWEL__ ' +
                  '-U__ARMEL__ -U__THUMBEL__ -U__SOFTFP__ ' +
                  '-U__VFP_FP__ -U__ARM_NEON__ -U__ARM_FP16 ' +
                  '-U__ARM_FP16__ -U__MAVERICK__ -U__XSCALE__ -U__IWMMXT__ ' +
                  '-U__ARM_EABI__',

  'BIAS_NONE'   : '${REMOVE_BIAS}',
  'BIAS_ARM'    : '',
  'BIAS_X8632'  : '${REMOVE_BIAS} ' +
                  '-D__i386__ -D__i386 -D__i686 -D__i686__ -D__pentium4__',
  'BIAS_X8664'  : '${REMOVE_BIAS} ' +
                  '-D__amd64__ -D__amd64 -D__x86_64__ -D__x86_64 -D__core2__',

  'CC_FLAGS'    : '${OPT_LEVEL} -fuse-llvm-va-arg -Werror-portable-llvm ' +
                  '-nostdinc -DNACL_LINUX=1 -D__native_client__=1 ' +
                  '-D__pnacl__=1 ${BIAS_%BIAS%}',
  'CC_STDINC'   :
    # NOTE: the two competing approaches here
    #       make the gcc driver "right" or
    #       put all the logic/knowloedge into this driver.
    #       Currently, we have a messy mixture.
    '-isystem ${BASE_ARM}/lib/gcc/arm-none-linux-gnueabi/4.2.1/include ' +
    '-isystem ' +
    '${BASE_ARM}/lib/gcc/arm-none-linux-gnueabi/4.2.1/install-tools/include ' +
    '-isystem ${BASE_ARM}/include/c++/4.2.1 ' +
    '-isystem ${BASE_ARM}/include/c++/4.2.1/arm-none-linux-gnueabi ' +
    '-isystem ${BASE_ARM_INCLUDE} ' +
    # NOTE: order important
    # '-isystem',
    # BASE + '/arm-newlib/arm-none-linux-gnueabi/usr/include/nacl/abi',
    '-isystem ${BASE}/arm-newlib/arm-none-linux-gnueabi/include',

  # Sandboxed tools
  'SCONS_OUT'           : '${BASE_NACL}/scons-out',
  'SCONS_STAGING_X8632' : '${SCONS_OUT}/opt-${SCONS_OS}-x86-32/staging',
  'SCONS_STAGING_X8664' : '${SCONS_OUT}/opt-${SCONS_OS}-x86-64/staging',
  'SCONS_STAGING_ARM'   : '${SCONS_OUT}/opt-${SCONS_OS}-arm/staging',

  'SCONS_OS'            : '${SCONS_OS_%BUILD_OS%}',
  'SCONS_OS_linux'      : 'linux',
  'SCONS_OS_darwin'     : 'mac',

  'USE_EMULATOR'        : '0',
  'SEL_UNIVERSAL_PREFIX': '',
  'SEL_UNIVERSAL_X8632' : '${SCONS_STAGING_X8632}/sel_universal',
  'SEL_UNIVERSAL_X8664' : '${SCONS_STAGING_X8664}/sel_universal',
  'SEL_UNIVERSAL_ARM'   : '${SCONS_STAGING_ARM}/sel_universal',
  'SEL_UNIVERSAL_FLAGS' : '--abort_on_error',

  'EMULATOR_ARM'        : '${BASE_TRUSTED}/run_under_qemu_arm',

  # TODO(pdox): Clean this up so there is one define for each item
  #             instead of 3.
  'SEL_LDR_X8632' : '${SCONS_STAGING_X8632}/sel_ldr',
  'SEL_LDR_X8664' : '${SCONS_STAGING_X8664}/sel_ldr',
  'SEL_LDR_ARM'   : '${SCONS_STAGING_ARM}/sel_ldr',

  'BASE_SB_X8632' : '${BASE}/tools-sb/x8632',
  'BASE_SB_X8664' : '${BASE}/tools-sb/x8664',
  'BASE_SB_ARM'   : '${BASE}/tools-sb/arm',

  'LLC_SRPC_X8632': '${BASE_SB_X8632}/srpc/bin/llc',
  'LLC_SRPC_X8664': '${BASE_SB_X8664}/srpc/bin/llc',
  'LLC_SRPC_ARM'  : '${BASE_SB_ARM}/srpc/bin/llc',

  'LD_SRPC_X8632': '${BASE_SB_X8632}/srpc/bin/ld',
  'LD_SRPC_X8664': '${BASE_SB_X8664}/srpc/bin/ld',
  'LD_SRPC_ARM'  : '${BASE_SB_ARM}/srpc/bin/ld',

  'LLC_SB_X8632'  : '${SEL_LDR_X8632} -a -- ${BASE_SB_X8632}/nonsrpc/bin/llc',
  'AS_SB_X8632'   : '${SEL_LDR_X8632} -a -- ${BASE_SB_X8632}/nonsrpc/bin/as',
  'LD_SB_X8632'   : '${SEL_LDR_X8632} -a -- ${BASE_SB_X8632}/nonsrpc/bin/ld',

  'LLC_SB_X8664'  : '${SEL_LDR_X8664} -a -- ${BASE_SB_X8664}/nonsrpc/bin/llc',
  'AS_SB_X8664'   : '${SEL_LDR_X8664} -a -- ${BASE_SB_X8664}/nonsrpc/bin/as',
  'LD_SB_X8664'   : '${SEL_LDR_X8664} -a -- ${BASE_SB_X8664}/nonsrpc/bin/ld',

  'LLC_SB_ARM'    : '${SEL_LDR_ARM} -a -- ${BASE_SB_ARM}/nonsrpc/bin/llc',
  'AS_SB_ARM'     : '${SEL_LDR_ARM} -a -- ${BASE_SB_ARM}/nonsrpc/bin/as',
  'LD_SB_ARM'     : '${SEL_LDR_ARM} -a -- ${BASE_SB_ARM}/nonsrpc/bin/ld',

  'LLVM_MC'       : '${BASE_ARM}/bin/llvm-mc',
  'LLVM_LD'       : '${BASE_ARM}/bin/llvm-ld',
  'LLVM_AS'       : '${BASE_ARM}/bin/llvm-as',
  'LLVM_DIS'      : '${BASE_ARM}/bin/llvm-dis',
  'LLVM_LINK'     : '${BASE_ARM}/bin/llvm-link',

  'LLC'           : '${BASE_ARM}/bin/llc',

  'LLC_FLAGS_COMMON' : '-asm-verbose=false',
  'LLC_FLAGS_ARM' :
    # The following options might come in hand and are left here as comments:
    # TODO(robertm): describe their purpose
    #     '-soft-float -aeabi-calls -sfi-zero-mask',
    # NOTE: we need a fairly high fudge factor because of
    # some vfp instructions which only have a 9bit offset
    ('-arm-reserve-r9 -sfi-disable-cp -arm_static_tls ' +
     '-sfi-store -sfi-stack -sfi-branch -sfi-data ' +
     '-no-inline-jumptables'),
  'LLC_FLAGS_X8632' : '',
  'LLC_FLAGS_X8664' : '',

  # LLC flags which set the target and output type.
  # These are handled separately by libLTO.
  'LLC_FLAGS_TARGET' : '-march=${LLC_MARCH_%arch%} -mcpu=${LLC_MCPU_%arch%} ' +
                       '-mtriple=${TRIPLE_%arch%} -filetype=${filetype}',
  'LLC_FLAGS_BASE': '${LLC_FLAGS_COMMON} ${LLC_FLAGS_%arch%}',
  'LLC_FLAGS'     : '${LLC_FLAGS_TARGET} ${LLC_FLAGS_BASE}',

  'LLC_MARCH_ARM'   : 'arm',
  'LLC_MARCH_X8632' : 'x86',
  'LLC_MARCH_X8664' : 'x86-64',

  'LLC_MCPU_ARM'    : 'cortex-a8',
  'LLC_MCPU_X8632'  : 'pentium4',
  'LLC_MCPU_X8664'  : 'core2',

  'OPT'      : '${BASE_ARM}/bin/opt',
  'OPT_FLAGS': '-std-compile-opts -O3',
  'OPT_LEVEL': '',

  'BINUTILS_BASE' : '${BASE_ARM}/bin/arm-pc-nacl-',
  'OBJDUMP_ARM'   : '${BINUTILS_BASE}objdump',
  'OBJDUMP_X8632' : '${BINUTILS_BASE}objdump',
  'OBJDUMP_X8664' : '${BINUTILS_BASE}objdump',

  'AS_ARM'        : '${BINUTILS_BASE}as',
  'AS_X8632'      : '${BASE_TOOLCHAIN}/${SCONS_OS}_x86_newlib/bin/nacl-as',
  'AS_X8664'      : '${BASE_TOOLCHAIN}/${SCONS_OS}_x86_newlib/bin/nacl64-as',

  'AS_FLAGS_ARM'  : '-mfpu=vfp -march=armv7-a',
  'AS_FLAGS_X8632': '--32 --nacl-align 5 -n -march=pentium4 -mtune=i386',
  'AS_FLAGS_X8664': '--64 --nacl-align 5 -n -mtune=core2',

  'MC_FLAGS_ARM'   : '-assemble -filetype=obj -arch=arm -triple=armv7a-nacl',
  'MC_FLAGS_X8632' : '-assemble -filetype=obj -arch=i686 -triple=i686-nacl',
  'MC_FLAGS_X8664' : '-assemble -filetype=obj -arch=x86_64 -triple=x86_64-nacl',

  'NM'             : '${BINUTILS_BASE}nm',
  'AR'             : '${BINUTILS_BASE}ar',
  'RANLIB'         : '${BINUTILS_BASE}ranlib',

  'LD'             : '${LD_%WHICH_LD%} ${LD_%WHICH_LD%_FLAGS}',

  'LD_BFD'         : '${BINUTILS_BASE}ld.bfd',
  'LD_BFD_FLAGS'   : '-m ${LD_EMUL_%arch%} -T ${LD_SCRIPT_%ARCH%}',

  'LD_GOLD_FLAGS'  : '--native-client',
  'LD_GOLD'        : '${BINUTILS_BASE}ld.gold',

  'LD_WRAP_SYMBOLS': '',
  'LD_FLAGS'       : '-nostdlib ${LD_SEARCH_DIRS}',
  'LD_SEARCH_DIRS' : '',

  'LD_EMUL_ARM'    : 'armelf_nacl',
  'LD_EMUL_X8632'  : 'elf_nacl',
  'LD_EMUL_X8664'  : 'elf64_nacl',

  'LDSCRIPTS_DIR'  : '${BASE}/ldscripts',
  'LD_SCRIPT_ARM'  : '${LDSCRIPTS_DIR}/ld_script_arm_untrusted',
  'LD_SCRIPT_X8632': '${LDSCRIPTS_DIR}/ld_script_x8632_untrusted',
  'LD_SCRIPT_X8664': '${LDSCRIPTS_DIR}/ld_script_x8664_untrusted',

  'BCLD'      : '${BINUTILS_BASE}ld.gold',
  'BCLD_FLAGS': '--native-client --undef-sym-check -T ${LD_SCRIPT_X8632} ' +
                '${GOLD_PLUGIN_ARGS} ${LD_SEARCH_DIRS}',

  # TODO(robertm): move this into the TC once this matures
  'PEXE_ALLOWED_UNDEFS': 'tools/llvm/non_bitcode_symbols.txt',

  'STDLIB_NATIVE_PREFIX': '${ROOT_%arch%}/crt1.o',

  'STDLIB_NATIVE_SUFFIX': '${ROOT_%arch%}/libcrt_platform.a ' +
                          '-L${ROOT_%arch%} -lgcc_eh -lgcc ',

  'STDLIB_BC_PREFIX': '${ROOT_BC}/nacl_startup.bc',

  'STDLIB_BC_SUFFIX':  '-L${ROOT_BC} -lc -lnacl ${LIBSTDCPP}',

  'LIBSTDCPP'       : '-lstdc++',

  # Build actual command lines below
  'RUN_OPT': '${OPT} ${OPT_FLAGS} "${input}" -f -o "${output}"',

  'RUN_LLC': '${LLC} ${LLC_FLAGS} ' +
             '"${input}" -o "${output}"',

  'RUN_LLVM_AS'  : '${LLVM_AS} "${input}" -o "${output}"',
  'RUN_LLVM_LINK': '${LLVM_LINK} ${inputs} -o "${output}"',

  'RUN_NACL_AS' : '${AS_%arch%} ${AS_FLAGS_%arch%} "${input}" -o "${output}"',
  'RUN_LLVM_MC' : '${LLVM_MC} ${MC_FLAGS_%arch%} "${input}" -o "${output}"',

  'RUN_GCC': '${CC} -emit-llvm ${mode} ${CC_FLAGS} ${CC_STDINC} ' +
             '-fuse-llvm-va-arg "${input}" -o "${output}"',

  'RUN_PP' : '${CC} -E ${CC_FLAGS} ${CC_STDINC} ' +
             '"${input}" -o "${output}"',

  'RUN_LLVM_DIS'    : '${LLVM_DIS} "${input}" -o "${output}"',
  'RUN_NATIVE_DIS'  : '${OBJDUMP_%arch%} -d "${input}"',

  # Multiple input actions
  'LD_INPUTS' : '${STDLIB_NATIVE_PREFIX} ${inputs} ${STDLIB_NATIVE_SUFFIX}',
  'RUN_LD' : '${LD} ${LD_FLAGS} ${LD_INPUTS} -o "${output}"',

  'RUN_BCLD': ('${BCLD} ${BCLD_FLAGS} '
               '${STDLIB_NATIVE_PREFIX} ${STDLIB_BC_PREFIX} '
               '${obj_inputs} '
               '--start-group '
               '${lib_inputs} '
               '${STDLIB_BC_SUFFIX} '
               '--end-group '
               '${STDLIB_NATIVE_SUFFIX} '
               '${BASE}/llvm-intrinsics.bc '
               '-o "${output}"'),

  'RUN_PEXECHECK': '${LLVM_LD} --nacl-abi-check ' +
                   '--nacl-legal-undefs ${PEXE_ALLOWED_UNDEFS} ${inputs}',
}

######################################################################
# Patterns for command-line arguments
######################################################################
DriverPatterns = [
  ( '--pnacl-driver-verbose',          "Log.LOG_OUT.append(sys.stderr)"),
  ( '--pnacl-driver-save-temps',       "env.set('CLEANUP', '0')"),
  ( '--driver=(.+)',                   "env.set('LLVM_GCC', $0)\n"
                                       "env.set('LLVM_GXX', $0)"),
  ( '--pnacl-driver-set-([^=]+)=(.*)', "env.set($0, $1)"),
  ( '--pnacl-driver-append-([^=]+)=(.*)', "env.append($0, $1)"),
  ( ('-arch', '(.+)'),                 "env.set('ARCH', FixArch($0))"),
  ( '--pnacl-as',                      "env.set('INCARNATION', 'as')"),
  ( '--pnacl-dis',                     "env.set('INCARNATION', 'dis')"),
  ( '--pnacl-gcc',                     "env.set('INCARNATION', 'gcc')"),
  ( '--pnacl-ld',                      "env.set('INCARNATION', 'ld')"),
  ( '--pnacl-opt',                     "env.set('INCARNATION', 'opt')"),
  ( '--pnacl-strip',                   "env.set('INCARNATION', 'strip')"),
  ( '--pnacl-translate',               "env.set('INCARNATION', 'translate')"),
  ( ('--add-llc-option', '(.+)'),      "env.append('LLC_FLAGS_COMMON', $0)"),
  ( '--pnacl-sb',                      "env.set('SANDBOXED', '1')"),
  ( '--pnacl-use-emulator',            "env.set('USE_EMULATOR', '1')"),
  ( '--dry-run',                       "env.set('DRY_RUN', '1')"),
  ( '--pnacl-gold-fix',                "env.set('GOLD_FIX', '1')"),
  ( '--pnacl-arm-bias',                "env.set('BIAS', 'ARM')"),
  ( '--pnacl-i686-bias',               "env.set('BIAS', 'X8632')"),
  ( '--pnacl-x86_64-bias',             "env.set('BIAS', 'X8664')"),
  ( '--pnacl-bias=(.+)',               "env.set('BIAS', FixArch($0))"),
  ( '--pnacl-skip-ll',                 "env.set('EMIT_LL', '0')"),
  ( '--pnacl-skip-lto',                "env.set('SKIP_OPT', '1')"),
  ( '--pnacl-force-mc',                "env.set('FORCE_MC', '1')"),
  ( '--pnacl-force-mc-direct',         "env.set('FORCE_MC_DIRECT', '1')"),

  # Catch all pattern (must be last)
  ( '(.*)',                            "env.append('ARGV', $0)"),
 ]

GCCPatterns = [
  ( '-o(.+)',          "env.set('OUTPUT', $0)"),
  ( ('-o', '(.+)'),    "env.set('OUTPUT', $0)"),

  ( '-E',              "env.set('GCC_MODE', '-E')"),
  ( '-S',              "env.set('GCC_MODE', '-S')"),
  ( '-c',              "env.set('GCC_MODE', '-c')"),

  # NOTE: For scons tests, the code generation fPIC flag is used with pnacl-ld.
  ( '-fPIC',           "env.set('PIC', '1')"),
  ( '-nostdinc',       "env.set('NOSTDINC', '1')"),
  # Different semantics for gcc and ld, but they both accept this flag.
  ( '-nostdlib',          "env.set('NOSTDLIB', '1')"),

  # Call ForceFileType for all input files at the time they are
  # parsed on the command-line. This ensures that the gcc "-x"
  # setting is correctly applied.
  ( '(.+\\.c)',        "env.append('INPUTS', $0); ForceFileType($0)"),
  ( '(.+\\.cc)',       "env.append('INPUTS', $0); ForceFileType($0)"),
  ( '(.+\\.cpp)',      "env.append('INPUTS', $0); ForceFileType($0)"),
  ( '(.+\\.C)',        "env.append('INPUTS', $0); ForceFileType($0)"),
  ( '(.+\\.s)',        "env.append('INPUTS', $0); ForceFileType($0)"),
  ( '(.+\\.S)',        "env.append('INPUTS', $0); ForceFileType($0)"),
  ( '(.+\\.asm)',      "env.append('INPUTS', $0); ForceFileType($0)"),
  ( '(.+\\.bc)',       "env.append('INPUTS', $0); ForceFileType($0)"),
  ( '(.+\\.o)',        "env.append('INPUTS', $0); ForceFileType($0)"),
  ( '(.+\\.os)',       "env.append('INPUTS', $0); ForceFileType($0)"),
  ( '(.+\\.ll)',       "env.append('INPUTS', $0); ForceFileType($0)"),
  ( '(.+\\.nexe)',     "env.append('INPUTS', $0); ForceFileType($0)"),
  ( '(.+\\.pexe)',     "env.append('INPUTS', $0); ForceFileType($0)"),
  ( '(-l.+)',          "env.append('INPUTS', $0)"),

  # This is currently only used for the front-end, but we may want to have
  # this control LTO optimization level as well.
  ( '(-O.+)',          "env.set('OPT_LEVEL', $0)"),

  # TODO(pdox): Figure out a way to only accept LD flags that we handle
  # instead of allowing everything through this.
  ( ('-Xlinker','(.*)'),  "env.append('LD_FLAGS', $0)\n"
                          "env.append('BCLD_FLAGS', $0)"),
  ( '-Wl,(.*)',           "env.append('LD_FLAGS', *($0.split(',')))\n"
                          "env.append('BCLD_FLAGS', *($0.split(',')))"),

  ( '(-g)',            "env.append('CC_FLAGS', $0)"),
  ( '(-W.*)',          "env.append('CC_FLAGS', $0)"),
  ( '(-std=.*)',       "env.append('CC_FLAGS', $0)"),
  ( '(-B.*)',          "env.append('CC_FLAGS', $0)"),

  ( '(-D.*)',                 "env.append('CC_FLAGS', $0)"),
  ( '(-f.*)',                 "env.append('CC_FLAGS', $0)"),
  ( ('-I', '(.+)'),           "env.append('CC_FLAGS', '-I' + $0)"),
  ( '(-I.+)',                 "env.append('CC_FLAGS', $0)"),
  ( '(-pedantic)',            "env.append('CC_FLAGS', $0)"),
  ( ('(-isystem)', '(.+)'),   "env.append('CC_FLAGS', $0, $1)"),
  ( '-shared',                "env.set('SHARED', '1')"),
  ( '-static',                "env.set('STATIC', '1')"),
  ( '(-g.*)',                 "env.append('CC_FLAGS', $0)"),
  ( '(-xassembler-with-cpp)', "env.append('CC_FLAGS', $0)"),

  ( ('-L', '(.+)'),           "env.append('LD_SEARCH_DIRS', '-L' + $0);"
                              "env.append('SEARCH_DIRS', $0)"),
  ( '-L(.+)',                 "env.append('LD_SEARCH_DIRS', '-L' + $0);"
                              "env.append('SEARCH_DIRS', $0)"),
  ( '(-Wp,.*)',               "env.append('CC_FLAGS', $0)"),

  ( '(-MMD)',                 "env.append('CC_FLAGS', $0)"),
  ( '(-MP)',                  "env.append('CC_FLAGS', $0)"),
  ( '(-MD)',                  "env.append('CC_FLAGS', $0)"),
  ( ('(-MF)','(.*)'),         "env.append('CC_FLAGS', $0, $1)"),
  ( ('(-MT)','(.*)'),         "env.append('CC_FLAGS', $0, $1)"),

  # With -xc, GCC considers future inputs to be an input source file.
  # The following is not the correct handling of -x,
  # but it works in the limited case used by llvm/configure.
  # TODO(pdox): Make this really work in the general case.
  ( '-xc',                    "env.append('CC_FLAGS', '-xc')\n"
                              "SetForcedFileType('src')"),

  # Ignore these gcc flags
  ( '(-msse)',                ""),
  ( '(-march=armv7-a)',       ""),

  # These are actually linker flags, not gcc flags
  # TODO(pdox): Move these into a separate linker patterns list
  ( ('-e','(.*)'),            "env.append('LD_FLAGS', '-e', $0)"),
  ( ('-Ttext','(.*)'),        "env.append('LD_FLAGS', '-Ttext', $0)"),
  ( ('(--section-start)','(.*)'), "env.append('LD_FLAGS', $0, $1)"),
  ( '(-soname=.*)',           "env.append('LD_FLAGS', $0)"),
  # NOTE: "-s" is also a gcc flag that is similar in spirit, but we'll
  # just use it as an LD_FLAG.
  ( '-s',                     "env.append('LD_FLAGS', '-s')"),
  ( '--strip-all',            "env.append('LD_FLAGS', '--strip-all')"),
  # NOTE: the ld -S flag currently conflicts with the gcc -S flag, so omit.
  #( '-S',                     "env.append('LD_FLAGS', '-S')"),
  ( '--strip-debug',          "env.append('LD_FLAGS', '--strip-debug')"),
  ( '-melf_nacl',             "env.set('ARCH', 'X8632')"),
  ( ('-m','elf_nacl'),        "env.set('ARCH', 'X8632')"),
  ( '-melf64_nacl',           "env.set('ARCH', 'X8664')"),
  ( ('-m','elf64_nacl'),      "env.set('ARCH', 'X8664')"),
  ( '-marmelf_nacl',          "env.set('ARCH', 'ARM')"),
  ( ('-m','armelf_nacl'),     "env.set('ARCH', 'ARM')"),

  # Ignore these assembler flags
  ( '(-Qy)',                  ""),
  ( ('(--traditional-format)', '.*'), ""),
  ( '(-gstabs)',              ""),
  ( '(--gstabs)',             ""),
  ( '(-gdwarf2)',             ""),
  ( '(--gdwarf2)',             ""),
  ( '(--fatal-warnings)',     ""),
  ( '(-meabi=.*)',            ""),
  ( '(-mfpu=.*)',             ""),

  # GCC diagnostic mode triggers
  ( '(-print-.*)',            "env.set('DIAGNOSTIC', '1')"),
  ( '(--print.*)',            "env.set('DIAGNOSTIC', '1')"),
  ( '(-dumpspecs)',           "env.set('DIAGNOSTIC', '1')"),
  ( '(-v|--version|--v|-V)',  "env.set('DIAGNOSTIC', '1')"),
  ( '(-d.*)',                 "env.set('DIAGNOSTIC', '1')"),

  # Catch all pattern (must be last)
  ( '(.*)',                   "if ForcedFileType:\n"
                              "  env.append('INPUTS', $0)\n"
                              "  ForceFileType($0)\n"
                              "else:\n"
                              "  env.append('UNMATCHED', $0)"),

]

def FixArch(arch):
  archfix = { 'x86-32': 'X8632',
              'x86_32': 'X8632',
              'x8632' : 'X8632',
              'i686'  : 'X8632',
              'ia32'  : 'X8632',

              'amd64' : 'X8664',
              'x86_64': 'X8664',
              'x86-64': 'X8664',
              'x8664' : 'X8664',

              'arm'   : 'ARM',
              'armv7' : 'ARM' }
  if arch not in archfix:
    Log.Fatal('Unrecognized arch "%s"!', arch)
  return archfix[arch]

def ClearStandardLibs():
  env.clear('STDLIB_NATIVE_PREFIX')
  env.clear('STDLIB_NATIVE_SUFFIX')
  env.clear('STDLIB_BC_PREFIX')
  env.clear('STDLIB_BC_SUFFIX')

def PrepareFlags():
  if env.getbool('PIC'):
    env.append('LLC_FLAGS_COMMON', '-relocation-model=pic')
    env.append('LLC_FLAGS_ARM', '-arm-elf-force-pic')

  if env.getbool('USE_EMULATOR'):
    env.append('SEL_UNIVERSAL_FLAGS', '-Q')
    env.append('SEL_UNIVERSAL_FLAGS', '--command_prefix', '${EMULATOR_%arch%}')
    env.append('SEL_UNIVERSAL_PREFIX', '${EMULATOR_%arch%}')

  if env.getbool('NOSTDINC'):
    env.clear('CC_STDINC')

  if env.getbool('NOSTDLIB'):
    ClearStandardLibs()

  if env.getbool('SANDBOXED'):
    env.set('SANDBOXED_LLC', '1')
    env.set('SANDBOXED_AS', '1')
    env.set('SANDBOXED_LD', '1')

  # Disable MC and sandboxed as/ld for ARM until we have it working
  arch = GetArch()
  if arch and arch == 'ARM':
    env.set('USE_MC_ASM', '0')
    if env.getbool('PIC'):
      env.set('MC_DIRECT', '0')
    env.set('SANDBOXED_AS', '0')
    env.set('SANDBOXED_LD', '0')

  ## Following two options are for ARM testing only
  #  Enable both direct object emission and using llvm-mc as native assembler
  if env.has('FORCE_MC'):
    env.set('MC_DIRECT', '1')
    env.set('USE_MC_ASM', '1')

  # Only enable direct object emission for ARM
  if env.has('FORCE_MC_DIRECT'):
    env.set('MC_DIRECT', '1')

  # Copy LD flags related to stripping over to OPT to have the analogous
  # actions be done to the bitcode.
  linkflags = shell.split(env.get('LD_FLAGS'))
  if '-s' in linkflags or '--strip-all' in linkflags:
    env.append('OPT_FLAGS', '-strip')
  if '-S' in linkflags or '--strip-debug' in linkflags:
    env.append('OPT_FLAGS', '-strip-debug')

  if env.getbool('SHARED'):
    env.append('LD_FLAGS', '-shared')
    env.clear('STDLIB_BC_PREFIX')
    env.clear('STDLIB_BC_SUFFIX')
    env.clear('STDLIB_NATIVE_PREFIX')
    env.clear('STDLIB_NATIVE_SUFFIX')
  else:
    # TODO(pdox): Figure out why this is needed.
    if env.getbool('GOLD_FIX'):
      env.append('LD_GOLD_FLAGS', '-static', '-T', '${LD_SCRIPT_%ARCH%}')
    else:
      env.append('LD_GOLD_FLAGS', '-static', '-Ttext=0x00020000')
    env.append('LD_BFD_FLAGS', '-static')

  if arch:
    if env.getbool('SANDBOXED_LLC'):
      env.set('LLC', '${LLC_SB_%s}' % arch)
    if env.getbool('SANDBOXED_AS'):
      env.set('AS', '${AS_SB_%s}' % arch)
    if env.getbool('SANDBOXED_LD'):
      env.set('LD', '${LD_SB_%s} ${LD_BFD_FLAGS}' % arch)

  # If --wrap is in LD_FLAGS, prepare to handle it ourselves
  # instead of passing onto the linker. Store the wrapped
  # symbol list in LD_WRAP_SYMBOLS.
  ld_flags = shell.split(env.get('LD_FLAGS'))
  symbols,ld_flags = ExtractWrapSymbols(ld_flags)
  env.set('LD_FLAGS', shell.join(ld_flags))
  env.set('LD_WRAP_SYMBOLS', shell.join(symbols))
  if len(symbols) > 0:
    bcld_flags = shell.split(env.get('BCLD_FLAGS'))
    _,bcld_flags = ExtractWrapSymbols(bcld_flags)
    env.set('BCLD_FLAGS', shell.join(bcld_flags))

def ExtractWrapSymbols(ld_flags):
  symbols = []
  leftover = []
  for arg in ld_flags:
    if arg.startswith('--wrap='):
      symbols.append(arg[len('--wrap='):])
    else:
      leftover.append(arg)
  return (symbols,leftover)


######################################################################
# Chain Map
######################################################################

# Order is very important here! These rules are scanned in order.
# The first rule seen that brings us "closer" to the output type is
# the one chosen.
#
# Closeness is determined by using ChainList. For example, 'll' is closer
# to 'bc' than 'c', because 'll' occurs after 'c' but before 'bc'.
#
# For example, if we are trying to compile cc -> bc and EMIT_LL is true,
# then the rule c -> ll will be selected first because it is the first rule
# on the list which brings us further along the chain towards bc. Next,
# the ll -> bc rule will be used to complete the transformation. This produces
# an intermediate ll file.
#
# However, if EMIT_LL is false, then the c -> ll rule is moved downward.
# As a result, the c -> bc rule will be selected first, and no
# intermediate .ll file will be produced.
def PrepareChainMap():
  ChainOrder = [
    'src',
    'pp',
    'll',
    'bc',
    'pexe',
    'S',
    's',
    'o',
    'nexe',
    'bclib',
    'nlib',
  ]

  EmitLL = env.getbool('EMIT_LL')
  MCDirect = env.getbool('MC_DIRECT')
  UseMCAsm = env.getbool('USE_MC_ASM')
  UseSRPC = env.getbool('SANDBOXED_LLC') and env.getbool('SRPC')
  SrpcObjSB = UseSRPC and MCDirect
  SrpcAsmSB = UseSRPC

  ChainMapInit = [
    # Condition, Inputs -> Out , Function,                Environment
    (EmitLL,   'src -> ll'   , 'RunWithEnv("RUN_GCC")', { 'mode': '-S' }),
    (True,     'src -> bc'   , 'RunWithEnv("RUN_GCC")', { 'mode': '-c' }),
    (True,     'src -> ll'   , 'RunWithEnv("RUN_GCC")', { 'mode': '-S' }),
    (True,     'src -> pp'   , 'RunWithEnv("RUN_PP")', {}),
    (True,     'll -> bc'    , 'RunWithEnv("RUN_LLVM_AS")', {}),
    (SrpcObjSB,'bc|pexe -> o', 'RunLLCSRPC()', { 'filetype': 'obj' }),
    (MCDirect, 'bc|pexe -> o', 'RunWithEnv("RUN_LLC")', { 'filetype': 'obj' }),
    (SrpcAsmSB,'bc|pexe -> s', 'RunLLCSRPC()', { 'filetype': 'asm' }),
    (True,     'bc|pexe -> s', 'RunWithEnv("RUN_LLC")', { 'filetype': 'asm' }),
    (True,     'S -> s'      , 'RunWithEnv("RUN_PP")', {}),
    (UseMCAsm, 's -> o'      , 'RunWithEnv("RUN_LLVM_MC")', {}),
    (True,     's -> o'      , 'RunWithEnv("RUN_NACL_AS")', {}),
  ]

  ChainMap.init(ChainOrder, ChainMapInit)


######################################################################
# Main and Incarnations
######################################################################

def main(argv):
  SetupSignalHandlers()
  env.reset()
  Log.reset()
  Log.Banner(argv)

  # Parse driver arguments
  ParseArgs(argv[1:], DriverPatterns)

  # Pull the incarnation and/or arch from the filename
  # Examples: driver.py      (gcc incarnation, no arch)
  #           pnacl-ld       (ld incarnation, no arch)
  #           pnacl-i686-as  (as incarnation, i686 arch)
  if argv[0].endswith('.py'):
    incarnation = "gcc"
  else:
    tokens = os.path.basename(argv[0]).split('-')
    if env.has('INCARNATION'):
      incarnation = env.get('INCARNATION')
    else:
      incarnation = tokens[-1]
    if len(tokens) > 2:
      arch = FixArch(tokens[-2])
      env.set('ARCH', arch)

  if incarnation == 'g++':
    incarnation = 'gxx'

  func = globals().get('Incarnation_' + incarnation)
  if not func:
    Log.Fatal('Unknown incarnation: ' + incarnation)

  incarnation_argv = shell.split(env.get('ARGV'))
  return func(incarnation_argv)

def PrepareCompile():
  PrepareFlags()
  PrepareChainMap()

def AssertParseComplete():
  unmatched = env.get('UNMATCHED')
  if len(unmatched) > 0:
    Log.Fatal('Unrecognized parameters: ' + unmatched)

def GetArch(required = False):
  arch = None
  if env.has('ARCH'):
    arch = env.get('ARCH')

  if required and not arch:
    Log.Fatal('Missing -arch!')

  return arch

def Incarnation_opt(argv):
  # This probably only needs the input file type, output, and "-O" option?
  ParseArgs(argv, GCCPatterns)
  AssertParseComplete()
  PrepareCompile()

  inputs = shell.split(env.get('INPUTS'))
  output = env.get('OUTPUT')

  if len(inputs) != 1:
    Log.Fatal('Expected one input file for opt')
  if output == '':
    Log.Fatal('Output file must be specified')
  infile = inputs[0]

  # If the input is llvm assembly, return llvm assembly instead of binary.
  intype = FileType(infile)
  if intype == 'll':
    env.append('OPT_FLAGS', '-S')

  OptimizeBC(infile, output)
  return 0

def Incarnation_strip(argv):
  # We do not handle the usual commandline options of the "strip" command,
  # we just go ahead and do the equivalent of --strip-all to bitcode.
  env.set('OPT_FLAGS', '-disable-opt -strip')
  return Incarnation_opt(argv)

def Incarnation_gxx(argv):
  env.set('CC', '${LLVM_GXX}')
  Incarnation_cc_common(argv)

def Incarnation_gcc(argv):
  env.set('CC', '${LLVM_GCC}')
  env.clear('LIBSTDCPP')
  Incarnation_cc_common(argv)

def Incarnation_cc_common(argv):
  assert(env.has('CC'))
  ParseArgs(argv, GCCPatterns)

  # "configure", especially when run as part of a toolchain bootstrap
  # process, will invoke gcc with various diagnostic options and
  # parse the output. In these cases we do not alter the incoming
  # commandline. It is also important to not emit spurious messages.
  if env.getbool('DIAGNOSTIC'):
    RunWithLog(shell.split(env.get('CC')) + argv)
    return 0

  AssertParseComplete()
  PrepareCompile()

  inputs = shell.split(env.get('INPUTS'))
  output = env.get('OUTPUT')
  arch = GetArch()
  gcc_mode = env.get('GCC_MODE')
  output_type_map = {
    ('-E', True) : 'pp',
    ('-E', False): 'pp',
    ('-c', True) : 'bc',
    ('-c', False): 'o',
    ('-S', True) : 'll',
    ('-S', False): 's',
    ('',   True) : 'pexe',
    ('',   False): 'nexe' }
  output_type = output_type_map[(gcc_mode, arch is None)]
  if output == '' and output_type in ('pexe', 'nexe'):
    output = 'a.out'
  elif output == '' and output_type == 'pp':
    output = '-'
  elif output == '' and len(inputs) == 1:
    output = DefaultOutputName(inputs[0], output_type)

  Compile(arch, inputs, output, output_type)
  return 0

def Incarnation_ld(argv):
  ParseArgs(argv, GCCPatterns)
  AssertParseComplete()
  PrepareCompile()

  inputs = shell.split(env.get('INPUTS'))
  output = env.get('OUTPUT')
  arch = GetArch()

  # The ld incarnation has three different uses which requires
  # different behavior.
  # A) Bitcode Link Only, output is Bitcode File. (-arch is not specified)
  # B) Bitcode Link + Native Link. Output is native. (-arch is specified)
  # C) Native Link Only. Output is native (-arch is specified, no bitcode given)
  # We detect which mode is being invoked below.
  has_native = False
  has_bitcode = False
  if len(inputs) == 0:
    Log.Fatal("no input files")
  for f in inputs:
    intype = FileType(f)
    if intype in ['o','nlib']:
      has_native |= True
    elif intype in ['bc','bclib']:
      has_bitcode |= True
    else:
      Log.Fatal("Input file '%s' of unexpected type for linking" % f)

  if has_bitcode:
    if not arch:
      mode = 'A'
    else:
      mode = 'B'
  elif has_native:
    mode = 'C'
  else:
    Log.Fatal("No input objects")

  # The normal "ld" does not implicitly link in startup objects
  # and standard libraries. However, our toolchain is currently
  # being used with the expectation that when linking bitcode,
  # these will be linked in automatically.
  #
  # Our toolchain also needs to be able to do native-linking
  # without including crt*, for example, to build
  # dynamic shared libs.
  #
  # This is entirely due to the way that scons needs to use our toolchain.
  #
  # TODO(pdox): Clean this up.
  if mode == 'A':
    output_type = 'pexe'
  elif mode == 'B':
    output_type = 'nexe'
  else:
    if not arch:
      Log.Fatal("Native object or library passed to ld without -arch")
    ClearStandardLibs()
    output_type = 'nexe'


  Compile(arch, inputs, output, output_type)
  return 0

def Incarnation_nm(argv):
  RunWithLog(shell.split(env.get('NM')) + argv, errexit = False)
  return 0

# The ar/ranlib hacks is are an attempt to iron out problems with shared
# libraries e.g. duplicate symbols in different libraries, before we have
# fully functional shared libs in pnacl.
def ExperimentalArHack(argv):
  mode = argv[0]
  output = argv[1]
  inputs = argv[2:]
  # NOTE: The checks below are a little on the paranoid side
  #       in most cases exceptions can be tolerated.
  #       It is probably still worth having a look at them first
  #       so we assert on everything unusual for now.
  if mode in ['x']:
    # This is used by by newlib to repackage a bunch of given archives
    # into a new archive. It assumes all the objectfiles can be found
    # via the glob, *.o
    assert len(inputs) == 0
    shutil.copyfile(output, output.replace('..','').replace('/','_') + '.o')
    return 0
  elif mode in ['cru']:
    # NOTE: we treat cru just like rc which just so happens to work
    #       with newlib but does not work in the general case.
    #       However, due to some idiosyncrasiers in newlib we need to prune
    #       duplicates.
    inputs = list(set(inputs))
  elif mode not in ['rc']:
    Log.Fatal('Unexpected "ar" mode %s', mode)
  if not output.endswith('.a'):
    Log.Fatal('Unexpected "ar" lib %s', output)
  not_bitcode = [f for f in inputs if 'bc' != FileType(f)]
  # NOTE: end of paranoid checks
  for f in not_bitcode:
    # This is for the last remaining native lib build via scons: libcrt_platform
    if not f.endswith('tls.o') and not f.endswith('setjmp.o'):
      Log.Fatal('Unexpected "ar" arg %s', f)
  if not_bitcode:
    RunWithLog(shell.split(env.get('AR')) + argv, errexit = False)
  else:
    RunWithEnv('RUN_LLVM_LINK', inputs=shell.join(inputs), output=output)
  return 0

def ExperimentalRanlibHack(argv):
  assert len(argv) > 0 and not argv[-1].startswith('-')
  if IsBitcode(argv[-1]):
    Log.Info('SKIPPING ranlib for bitcode file: %s', argv[-1])
  else:
    RunWithLog(shell.split(env.get('RANLIB')) + argv, errexit = False)
  return 0

def Incarnation_ar(argv):
  if env.getbool('SINGLE_BC_LIBS'): return ExperimentalArHack(argv)

  RunWithLog(shell.split(env.get('AR')) + argv, errexit = False)
  return 0

def Incarnation_ranlib(argv):
  if env.getbool('SINGLE_BC_LIBS'): return ExperimentalRanlibHack(argv)
  RunWithLog(shell.split(env.get('RANLIB')) + argv, errexit = False)
  return 0

def Incarnation_translate(argv):
  ParseArgs(argv, GCCPatterns)
  AssertParseComplete()
  PrepareCompile()

  # NOTE: When we translate browser side we do not want to run an expensive
  # bitcode optimization pass. The optimizations should have already run when
  # the pexe was generated.
  env.set('SKIP_OPT', '1')
  inputs = shell.split(env.get('INPUTS'))
  output = env.get('OUTPUT')
  output_type = 'nexe'
  arch = GetArch(required=True)
  if len(inputs) != 1:
    Log.Fatal('Expecting one input file')

  if FileType(inputs[0]) not in ('bc', 'pexe'):
    Log.Fatal('Expecting input to be a bitcode file')

  if output == '':
    output = DefaultOutputName(inputs[0], output_type)
  Compile(arch, inputs, output, output_type)
  return 0

def Incarnation_as(argv):
  # Unmatched parameters should be treated as
  # assembly inputs by the "as" incarnation.
  arch = GetArch()
  if arch:
    output_type = 'o'
    SetForcedFileType('s')
  else:
    output_type = 'bc'
    SetForcedFileType('ll')

  ParseArgs(argv, GCCPatterns)

  if env.getbool('DIAGNOSTIC'):
    arch = GetArch(required = True)
    RunWithLog(shell.split(env.get('AS_'+arch)) + argv)
    return 0

  AssertParseComplete()
  PrepareCompile()

  inputs = shell.split(env.get('INPUTS'))
  output = env.get('OUTPUT')

  if len(inputs) != 1:
    Log.Fatal('Expecting one input file')

  if output == '':
    output = DefaultOutputName(inputs[0], output_type)
  Compile(arch, inputs, output, output_type)
  return 0

def Incarnation_bclink(argv):
  # This probably only needs the input file type patterns and output?
  ParseArgs(argv, GCCPatterns)
  AssertParseComplete()
  PrepareCompile()

  inputs = shell.split(env.get('INPUTS'))
  output = env.get('OUTPUT')
  if output == '':
    Log.Fatal('Please specify output name')
  RunWithEnv('RUN_LLVM_LINK',
             inputs=shell.join(inputs),
             output=output)
  return 0

def Incarnation_dis(argv):
  # This probably only needs the input file type patterns and output?
  ParseArgs(argv, GCCPatterns)
  AssertParseComplete()
  PrepareCompile()

  inputs = shell.split(env.get('INPUTS'))
  output = env.get('OUTPUT')
  if len(inputs) != 1:
    Log.Fatal('Expecting exactly one input file')
  intype = FileType(inputs[0])
  if output == '':
    output = '-'
  if intype in ('o', 'nexe'):
    arch = GetArch(required = True)
    RunWithEnv('RUN_NATIVE_DIS', input=inputs[0], arch=arch)
  elif intype in ('pexe', 'bc'):
    RunWithEnv('RUN_LLVM_DIS', input=inputs[0], output=output)
  else:
    Log.Fatal('Unknown file type')
  return 0

def Incarnation_illegal(unused_argv):
  # unused_argv does not contain argv[0], etc, so we refer to sys.argv here.
  Log.Fatal('ILLEGAL COMMAND: ' + StringifyCommand(sys.argv))

def Incarnation_nop(unused_argv):
  # unused_argv does not contain argv[0], etc, so we refer to sys.argv here.
  Log.Info('IGNORING: ' + StringifyCommand(sys.argv))
  NiceExit(0)

def Incarnation_pexecheck(argv):
  """Check whether a pexe satifies our ABI rules"""
  RunWithEnv('RUN_PEXECHECK', inputs=argv[0])

def NiceExit(code):
  sys.exit(code)

######################################################################
# Compiling / Linking Engine
######################################################################

def CompileOne(arch, outtype, infile, output = None):
  """Compile a single file all the way to type 'outtype'"""
  assert((output is None) or (output != ''))
  intype = FileType(infile)
  curfile = infile

  path = ChainMap.GetPath(intype, outtype)
  if path is None:
    Log.Fatal('Unable to find a compilation path!')
  for entry in path:
    if entry.output_type != outtype or output is None:
      # We are creating a temporary file
      nextfile = TempNameForInput(infile, entry.output_type)
    else:
      # We are emitting the final output
      nextfile = output

    env.push()
    env.setmany(arch=arch, input=curfile, output=nextfile, **entry.extra_env)
    try:
      exec(entry.cmd)
    except SystemExit, e:
      raise e
    except Exception, err:
      Log.Fatal('Chain action [%s] failed with: %s', entry.cmd, err)
    env.pop()
    curfile = nextfile

  return curfile


def Compile(arch, inputs, output, output_type):
  """Compile and/or link one or more files all the way to output_type"""

  if len(inputs) == 0:
    Log.Fatal('No input files')

  # If there are multiple input files and no linking is being done,
  # then there are multiple outputs. Handle this by recursively
  # calling Compile for each one.
  if len(inputs) > 1 and output_type not in ('nexe', 'pexe'):
    if output != '':
      Log.Fatal('Cannot have -o with -c, -S, or -E and multiple inputs')

    for infile in inputs:
      output = DefaultOutputName(infile, output_type)
      Compile(arch, [infile], output, output_type)
    return

  if output == '':
    Log.Fatal('Output file name must be specified')

  InitTempNames(inputs, output)

  # Handle single file, non-linking case
  if output_type not in ('nexe', 'pexe'):
    assert(len(inputs) == 1)
    CompileOne(arch, output_type, inputs[0], output)
    ClearTemps()
    return 0

  # Compile all source files (c/c++/ll) to .bc
  for i in xrange(0, len(inputs)):
    intype = FileType(inputs[i])
    if ChainMap.PathExists(intype, 'bc'):
      inputs[i] = CompileOne(arch, 'bc', inputs[i])

  # Compile all .s/.S/.pexe to .o
  if output_type == 'nexe':
    for i in xrange(0, len(inputs)):
      intype = FileType(inputs[i])
      if intype in ('s','S','pexe'):
        inputs[i] = CompileOne(arch, 'o', inputs[i])

  # We should only be left with .bc and .o and libraries
  for i in xrange(0, len(inputs)):
    intype = FileType(inputs[i])
    assert(intype in ('bc','pexe','o','nlib','bclib'))

  if output_type == 'nexe':
    LinkAll(arch, inputs, output)
    ClearTemps()
    return
  elif output_type == 'pexe':
    LinkBC(inputs, output)
    ClearTemps()
    return

  Log.Fatal('Unexpected output type')


def IsPrelinked(inputs):
  '''Determine if a pexe is present and return it or None.
  '''
  pexe = None
  for i in inputs:
    intype = FileType(i)
    if intype == 'pexe':
      assert pexe is None
      pexe = i
  return pexe


def ExtractNonBitcodeFiles(inputs):
  return [f for f in inputs if FileType(f) not in ('bc','bclib')]


def LinkAll(arch, inputs, output):
  '''Link a bunch of input files together into a native executable.
  If necessary run the llc backend.
  If necessary run the bitcode linking phase.
  '''
  if IsPrelinked(inputs):
    pexe_obj = CompileOne(arch, 'o', IsPrelinked(inputs))
    LinkNative(arch, [pexe_obj] + ExtractNonBitcodeFiles(inputs), output)
  elif NeedsBCLinking(inputs):
    pexe = LinkBC(inputs)
    pexe_obj = CompileOne(arch, 'o', pexe)
    LinkNative(arch, [pexe_obj] + ExtractNonBitcodeFiles(inputs), output)
  else:
    LinkNative(arch, inputs, output)


def NeedsBCLinking(inputs):
  '''Determine if any bc files are present
  Also make sure our inputs are only .bc, .o, and -l
  '''
  needs_linking = False
  for i in inputs:
    intype = FileType(i)
    # TODO(robertm): disallow .pexe and .o
    assert(intype in ('bc', 'o', 'bclib', 'nlib'))
    if intype == 'bc':
      needs_linking = True
  return needs_linking

def HasNativeObject(inputs):
  for i in inputs:
    intype = FileType(i)
    if intype == 'o':
      return True
  return False

def LinkBC(inputs, output = None):
  '''Input: a bunch of bc/o/lib input files
     Output: a combined & optimized bitcode file
     if output == None, a new temporary name will be chosen
     and returned.
  '''
  assert(output is None or output != '')

  skip_opt = env.getbool('SKIP_OPT')
  if skip_opt:
    if output is None:
      output = TempNameForOutput('pexe')
    bcld_output = output
  else:
    if output is None:
      output = TempNameForOutput('opt.pexe')
    bcld_output = TempNameForOutput('bc')

  # There is an architecture bias here. To link the bitcode together,
  # we need to specify the native libraries to be used in the final linking,
  # just so the linker can resolve the symbols. We'd like to eliminate
  # this somehow.
  arch = GetArch()
  if not arch:
    arch = 'X8632'

  # separate library input from non-library inputs
  obj_inputs = []
  lib_inputs = []
  for i in inputs:
    if FileType(i) in ('bclib','nlib'):
      lib_inputs.append(i)
    else:
      obj_inputs.append(i)

  # this transformation of eliminating duplicates should be harmless in general
  # but leads to perturbed command lines so we do not enable it by default
  if env.getbool('SINGLE_BC_LIBS'):
    lib_inputs = list(set(lib_inputs))

  # Produce combined bitcode file
  if env.getbool('SHARED'):
    RunWithEnv('RUN_LLVM_LINK',
               inputs=shell.join(obj_inputs),
               output=bcld_output)
  else:
    RunWithEnv('RUN_BCLD',
               arch=arch,
               obj_inputs=shell.join(obj_inputs),
               lib_inputs=shell.join(lib_inputs),
               output=bcld_output)

  #### Manually apply symbol wrap to unoptimized bitcode
  symbols = shell.split(env.get('LD_WRAP_SYMBOLS'))
  if len(symbols) > 0:
    before = TempNameForOutput('before_wrap.ll')
    after = TempNameForOutput('after_wrap.ll')
    RunWithEnv('RUN_LLVM_DIS', input=bcld_output, output=before)
    WrapSymbols(symbols, before, after)
    RunWithEnv('RUN_LLVM_AS', input=after, output=bcld_output)
  ####

  if not skip_opt:
    OptimizeBC(bcld_output, output)

  return output

def OptimizeBC(input, output):
  # Optimize combined bitcode file
  RunWithEnv('RUN_OPT',
             input = input,
             output = output)

# Final native linking step
def LinkNative(arch, inputs, output):
  # Make sure we are only linking native code
  for f in inputs:
    assert(FileType(f) in ('o', 'nlib'))

  env.push()
  env.setmany(arch=arch, inputs=shell.join(inputs), output=output)

  # TODO(pdox): Unify this into the chain compile
  if env.getbool('SANDBOXED_LD') and env.getbool('SRPC'):
    RunLDSRPC()
  else:
    RunWithEnv('RUN_LD')

  env.pop()
  return

######################################################################
# Chain Logic
######################################################################

class ChainEntry:
  def __init__(self, input_type, output_type, cmd, extra_env):
    self.input_type = input_type
    self.output_type = output_type
    self.cmd = cmd
    self.extra_env = extra_env

class ChainMap:
  Order = []           # File type order
  Rules = []           # List of ChainEntry's
  OrderLookup = dict() # Inverse map of 'Order'

  @classmethod
  def init(cls, ChainOrder, ChainMapInit):
    cls.Order = ChainOrder
    cls.OrderLookup = dict([ (b,a) for (a,b) in enumerate(cls.Order) ])

    # Convert ChainMapInit into Rules
    cls.Rules = []
    for (condition, pat, cmd, extra) in ChainMapInit:
      if not condition:
        continue
      pat = pat.replace(' ','')
      inputs, output = pat.split('->')
      inputs = inputs.split('|')
      for i in inputs:
        cls.Rules.append(ChainEntry(i, output, cmd, extra))

  # Find the next compilation step which will bring
  # intype closer to being outtype
  # If none exists, returns None
  @classmethod
  def NextStep(cls, intype, outtype):
    innum = cls.OrderLookup[intype]
    outnum = cls.OrderLookup[outtype]

    if innum >= outnum:
      return None
    for entry in cls.Rules:
      if entry.input_type != intype:
        continue
      if cls.OrderLookup[entry.output_type] > outnum:
        continue
      return entry
    return None

  # Returns a list of ChainEntry's which describe
  # what actions to perform to move from input_type
  # to output_type
  @classmethod
  def GetPath(cls, input_type, output_type):
    cur_type = input_type
    result = []
    while cur_type != output_type:
      entry = cls.NextStep(cur_type, output_type)
      if entry is None:
        return None
      result.append(entry)
      cur_type = entry.output_type
    return result

  @classmethod
  def PathExists(cls, input_type, output_type):
    path = cls.GetPath(input_type, output_type)
    return path is not None

######################################################################
# Environment
######################################################################

class env:
  data = []
  stack = []

  @classmethod
  def reset(cls):
    cls.data = dict(INITIAL_ENV)
    cls.set('BASE_NACL', FindBaseDir())
    cls.set('BUILD_OS', GetBuildOS())
    cls.set('BUILD_ARCH', GetBuildArch())

  @classmethod
  def dump(cls):
    for (k,v) in cls.data.iteritems():
      print '%s == %s' % (k,v)

  @classmethod
  def push(cls):
    cls.stack.append(cls.data)
    cls.data = dict(cls.data) # Make a copy

  @classmethod
  def pop(cls):
    cls.data = cls.stack.pop()

  @classmethod
  def has(cls, varname):
    return varname in cls.data

  # Retrieve and evaluate a variable from the environment
  @classmethod
  def get(cls, varname):
    return cls.eval(cls.data[varname])

  @classmethod
  def getbool(cls, varname):
    return bool(int(cls.get(varname)))

  # Set a variable in the environment
  @classmethod
  def set(cls, varname, val):
    cls.data[varname] = val

  # Set one or more variables using named arguments
  @classmethod
  def setmany(cls, **kwargs):
    for k,v in kwargs.iteritems():
      cls.data[k] = v

  @classmethod
  def clear(cls, varname):
    cls.data[varname] = ''

  # Append one or more terms to a variable in the environment
  @classmethod
  def append(cls, varname, *vals):
    x = ''
    for v in vals:
      x += ' ' + shell.escape(v)

    if not varname in cls.data:
      cls.data[varname] = ''
    else:
      cls.data[varname] += ' '
    cls.data[varname] += x.strip()

  # Evaluate an expression in the environment
  # Returns a string
  @classmethod
  def eval(cls, expr):
    lpos = expr.find('${')
    if lpos == -1:
      return expr
    rpos = expr.find('}', lpos)
    if rpos == -1:
      print 'Unterminated ${ starting at: ' + expr[lpos:]
      NiceExit(0)
    varname = expr[lpos+2:rpos]
    if '%' in varname:
      # This currently assumes only one '%' sub-expression
      sublpos = varname.find('%')
      subrpos = varname.rfind('%')
      assert(sublpos >= 0 and subrpos >= 0)
      sub = cls.eval('${' + varname[sublpos+1:subrpos] + '}')
      varname = varname[0:sublpos] + sub + varname[subrpos+1:]
    if varname not in cls.data:
      print 'Undefined variable name ' + varname
      NiceExit(1)
    return cls.eval(expr[0:lpos] + cls.data[varname] + expr[rpos+1:])

# Run a command with extra environment settings
def RunWithEnv(cmd, **kwargs):
  env.push()
  env.setmany(**kwargs)
  RunWithLog('${%s}' % cmd)
  env.pop()

def GetBuildOS():
  name = platform.system().lower()
  if name not in ('linux', 'darwin'):
    Log.Fatal("Unsupported platform '%s'" % (name,))
  return name

def GetBuildArch():
  return platform.machine()

# Crawl backwards, starting from the directory containing this script,
# until we find the native_client/ directory.
def FindBaseDir():
  Depth = 0
  cur = os.path.abspath(sys.argv[0])
  while os.path.basename(cur) != 'native_client' and \
        Depth < 16:
    cur = os.path.dirname(cur)
    Depth += 1

  if os.path.basename(cur) != "native_client":
    print "Unable to find native_client directory!"
    sys.exit(1)

  return cur


######################################################################
# Argument Parsing
######################################################################

def MatchOne(argv, i, patternlist):
  """Find a pattern which matches argv starting at position i"""
  for (regex, action) in patternlist:
    if isinstance(regex, str):
      regex = [regex]
    j = 0
    matches = []
    for r in regex:
      if i+j < len(argv):
        match = re.compile('^'+r+'$').match(argv[i+j])
      else:
        match = None
      matches.append(match)
      j += 1
    if None in matches:
      continue
    groups = [ list(m.groups()) for m in matches ]
    groups = reduce(lambda x,y: x+y, groups, [])
    return (len(regex), action, groups)
  return (0, '', [])

def ParseArgs(argv, patternlist):
  """Parse argv using the patterns in patternlist"""

  i = 0
  while i < len(argv):
    num_matched, action, groups = MatchOne(argv, i, patternlist)
    if num_matched == 0:
      Log.Fatal('Unrecognized argument: ' + argv[i])
    for g in xrange(0, len(groups)):
      action = action.replace('$%d' % g, 'groups[%d]' % g)
    try:
      # NOTE: this is essentially an eval for python expressions
      # which does rely on the current environment for unbound vars
      # Log.Info('about to exec [%s]', str(action))
      exec(action)
    except Exception, err:
      Log.Fatal('ParseArgs action [%s] failed with: %s', action, err)
    i += num_matched
  return

######################################################################
# File Naming Logic
######################################################################

def SimpleCache(f):
  """ Cache results of a one-argument function using a dictionary """
  cache = dict()
  def wrapper(arg):
    if arg in cache:
      return cache[arg]
    else:
      result = f(arg)
      cache[arg] = result
      return result
  wrapper.__name__ = f.__name__
  wrapper.__cache = cache
  return wrapper

@SimpleCache
def IsBitcode(filename):
  try:
    fp = open(filename, 'rb')
  except Exception:
    print "Failed to open input file " + filename
    NiceExit(1)
  header = fp.read(2)
  fp.close()
  if header == 'BC':
    return True
  return False

# If ForcedFileType is set, FileType() will return ForcedFileType for all
# future input files. This is useful for the "as" incarnation, which
# needs to accept files of any extension and treat them as ".s" (or ".ll")
# files. Also useful for gcc's "-x", which causes all files to be treated
# in a certain way.
ForcedFileType = None
def SetForcedFileType(t):
  global ForcedFileType
  ForcedFileType = t

def ForceFileType(filename, newtype = None):
  if newtype is None:
    if ForcedFileType is None:
      return
    newtype = ForcedFileType
  FileType.__cache[filename] = newtype

# The SimpleCache decorator is required for correctness, due to the
# ForceFileType mechanism.
@SimpleCache
def FileType(filename):
  if filename.startswith('-l'):
    # Determine whether this is a bitcode library or native .so
    path = FindLib(filename[2:], shell.split(env.get('SEARCH_DIRS') + " " +
                                             env.get('ROOT_BC')))
    if path is None:
      Log.Fatal("Cannot find library %s", filename)
    if path.endswith('.a') or IsBitcode(path):
      return 'bclib'
    return 'nlib'

  # Auto-detect bitcode files, since we can't rely on extensions
  ext = filename.split('.')[-1]
  if ext in ('o', 'so') and IsBitcode(filename):
    return 'bc'

  # File Extension -> Type string
  ExtensionMap = {
    'c'   : 'src',
    'cc'  : 'src',
    'cpp' : 'src',
    'C'   : 'src',
    'll'  : 'll',
    'bc'  : 'bc',
    'po'  : 'bc',   # .po = "Portable object file"
    'pexe': 'pexe', # .pexe = "Portable executable"
    'asm' : 'S',
    'S'   : 'S',
    's'   : 's',
    'o'   : 'o',
    'os'  : 'o',
    'nexe': 'nexe'
  }
  if ext not in ExtensionMap:
    Log.Fatal('Unknown file extension: %s' % filename)

  return ExtensionMap[ext]

def RemoveExtension(filename):
  if filename.endswith('.opt.bc'):
    return filename[0:-len('.opt.bc')]

  name, ext = os.path.splitext(filename)
  if ext == '':
    Log.Fatal('File has no extension: ' + filename)
  return name

def DefaultOutputName(filename, outtype):
  assert(outtype in ('pp','ll','bc','pexe','s','S','o','nexe','dis'))
  base = os.path.basename(filename)
  base = RemoveExtension(base)

  if outtype in ('pp','dis'): return '-'; # stdout
  if outtype in ('bc'): return base + '.o'
  return base + '.' + outtype

def PathSplit(f):
  paths = []
  cur = f
  while True:
    cur, piece = os.path.split(cur)
    if piece == '':
      break
    paths.append(piece)
  paths.reverse()
  return paths

# Generate a unique identifier for each input file.
# Start with the basename, and if that is not unique enough,
# add parent directories. Rinse, repeat.
TempBase = None
TempMap = None
TempList = []
def InitTempNames(inputs, output):
  global TempMap, TempBase
  inputs = [ os.path.abspath(i) for i in inputs ]
  output = os.path.abspath(output)

  TempMap = dict()
  TempBase = output + '---linked'

  # TODO(pdox): Figure out if there's a less confusing way
  #             to simplify the intermediate filename in this case.
  #if len(inputs) == 1:
  #  # There's only one input file, don't bother adding the source name.
  #  TempMap[inputs[0]] = output + '---'
  #  return

  # Build the initial mapping
  TempMap = dict()
  for f in inputs:
    path = PathSplit(f)
    TempMap[f] = [1, path]

  while True:
    # Find conflicts
    ConflictMap = dict()
    Conflicts = set()
    for (f, [n, path]) in TempMap.iteritems():
      candidate = output + '---' + '_'.join(path[-n:]) + '---'
      if candidate in ConflictMap:
        Conflicts.add(ConflictMap[candidate])
        Conflicts.add(f)
      else:
        ConflictMap[candidate] = f

    if len(Conflicts) == 0:
      break

    # Resolve conflicts
    for f in Conflicts:
      n = TempMap[f][0]
      if n+1 > len(TempMap[f][1]):
        Log.Fatal('Unable to resolve naming conflicts')
      TempMap[f][0] = n+1

  # Clean up the map
  NewMap = dict()
  for (f, [n, path]) in TempMap.iteritems():
    candidate = output + '---' + '_'.join(path[-n:]) + '---'
    NewMap[f] = candidate
  TempMap = NewMap
  return

def TempNameForOutput(imtype):
  global TempList, TempBase

  temp = TempBase + '.' + imtype
  TempList.append(temp)
  return temp

def TempNameForInput(input, imtype):
  global TempList
  fullpath = os.path.abspath(input)
  # If input is already a temporary name, just change the extension
  if fullpath.startswith(TempBase):
    temp = TempBase + '.' + imtype
  else:
    # Source file
    temp = TempMap[fullpath] + '.' + imtype

  TempList.append(temp)
  return temp

def ClearTemps():
  global TempList
  if env.getbool('CLEANUP'):
    for f in TempList:
      os.remove(f)
    TempList = []

######################################################################
# Shell Utilities
######################################################################

class shell:
  @staticmethod
  def split(s):
    """Split a shell-style string up into a list of distinct arguments.
    For example: split('cmd -arg1 -arg2="a b c"')
    Returns ['cmd', '-arg1', '-arg2=a b c']
    """

    out = []
    inspace = True
    inquote = False
    buf = ''

    i = 0
    while i < len(s):
      if s[i] == '"':
        inspace = False
        inquote = not inquote
      elif s[i] == ' ' and not inquote:
        if not inspace:
          out.append(buf)
          buf = ''
        inspace = True
      elif s[i] == '\\':
        if not i+1 < len(s):
          Log.Fatal('Unterminated \\ escape sequence')
        inspace = False
        i += 1
        buf += s[i]
      else:
        inspace = False
        buf += s[i]
      i += 1
    if inquote:
      Log.Fatal('Unterminated quote')
    if not inspace:
      out.append(buf)
    return out

  @staticmethod
  def join(args):
    """Turn a list into a shell-style string For example:
       shell.join([ 'a', 'b', 'c d e' ]) = 'a b "c d e"'
    """
    if isinstance(args, str):
      return args

    out = ''
    for a in args:
      out += shell.escape(a) + ' '

    return out[0:-1]

  @staticmethod
  def escape(s):
    """Shell-escape special characters in a string
       Surround with quotes if necessary
    """
    s = s.replace('\\', '\\\\')
    s = s.replace('"', '\\"')
    if ' ' in s:
      s = '"' + s + '"'
    return s

######################################################################
# Logging
######################################################################

class Log:
  # Lists of streams
  prefix = ''
  LOG_OUT = []
  ERROR_OUT = [sys.stderr]

  @classmethod
  def reset(cls):
    cls.prefix = '%d: ' % os.getpid()
    if env.getbool('LOG_TO_FILE'):
      filename = env.get('LOG_FILENAME')
      sizelimit = int(env.get('LOG_FILE_SIZE_LIMIT'))
      cls.AddFile(filename, sizelimit)

  @classmethod
  def AddFile(cls, filename, sizelimit):
    file_too_big = os.path.isfile(filename) and \
                   os.path.getsize(filename) > sizelimit
    mode = 'a'
    if file_too_big:
      mode = 'w'
    try:
      fp = open(filename, mode)
    except Exception:
      return
    cls.LOG_OUT.append(fp)

  @classmethod
  def Banner(cls, argv):
    cls.Info('PNaCl Driver Invoked With:\n' + StringifyCommand(argv))
    cls.Info('-' * 60)

  @classmethod
  def Info(cls, m, *args):
    cls.LogPrint(m, *args)

  @classmethod
  def FatalWithResult(cls, ret, m, *args):
    m = 'FATAL: ' + m
    cls.LogPrint(m, *args)
    cls.ErrorPrint(m, *args)
    NiceExit(ret)

  @classmethod
  def Fatal(cls, m, *args):
    # Note, using keyword args and arg lists while trying to keep
    # the m and *args parameters next to each other does not work
    cls.FatalWithResult(-1, m, *args)

  @classmethod
  def LogPrint(cls, m, *args):
    # NOTE: m may contain '%' if no args are given
    if args:
      m = m % args
    for o in cls.LOG_OUT:
      print >> o, m

  @classmethod
  def ErrorPrint(cls, m, *args):
    # NOTE: m may contain '%' if no args are given
    if args:
      m = m % args
    for o in cls.ERROR_OUT:
      print >> o, m

def EscapeEcho(s):
  """ Quick and dirty way of escaping characters that may otherwise be
      interpreted by bash / the echo command (rather than preserved). """
  return s.replace("\\", r"\\").replace("$", r"\$").replace('"', r"\"")

def StringifyCommand(cmd, stdin_contents=None):
  """ Return a string for reproducing the command "cmd", which will be
      fed stdin_contents through stdin. """
  stdin_str = ""
  if stdin_contents:
    stdin_str = "echo \"\"\"" + EscapeEcho(stdin_contents) + "\"\"\" | "
  if env.getbool('LOG_PRETTY_PRINT'):
    return stdin_str + PrettyStringify(cmd)
  else:
    return stdin_str + SimpleStringify(cmd)

def SimpleStringify(args):
  return " ".join(args)

def PrettyStringify(args):
  ret = ''
  grouping = 0
  for a in args:
    if grouping == 0 and len(ret) > 0:
      ret += " \\\n    "
    elif grouping > 0:
      ret += " "
    if grouping == 0:
      grouping = 1
      if a.startswith('-') and len(a) == 2:
        grouping = 2
    ret += a
    grouping -= 1
  return ret

# If the driver is waiting on a background process in RunWithLog()
# and the user Ctrl-C's or kill's the driver, it may leave
# the child process (such as llc) running. To prevent this,
# the code below sets up a signal handler which issues a kill to
# the currently running child processes.
CleanupProcesses = []
def SetupSignalHandlers():
  global CleanupProcesses
  def signal_handler(unused_signum, unused_frame):
    for p in CleanupProcesses:
      p.kill()
    sys.exit(127)
  if os.name == "posix":
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGHUP, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

def RunWithLog(args, stdin = None, silent = False, errexit = True):
  "Run the commandline give by the list args system()-style"

  if isinstance(args, str):
    args = shell.split(env.eval(args))

  Log.Info('\n' + StringifyCommand(args, stdin))

  if env.getbool('DRY_RUN'):
    return

  try:
    p = subprocess.Popen(args, stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE )
    CleanupProcesses.append(p)
    buf_stdout, buf_stderr = p.communicate(input = stdin)
    CleanupProcesses.pop()
    ret = p.returncode
  except SystemExit, e:
    raise e
  except Exception, e:
    buf_stdout = ''
    buf_stderr = ''
    Log.Fatal('failed (%s) to run: ' % str(e) + StringifyCommand(args, stdin))

  if not silent:
    sys.stdout.write(buf_stdout)
    sys.stderr.write(buf_stderr)

  if errexit and ret:
    Log.FatalWithResult(ret,
                        'failed command: %s\n'
                        'stdout        : %s\n'
                        'stderr        : %s\n',
                        StringifyCommand(args, stdin), buf_stdout, buf_stderr)
  else:
    Log.Info('Return Code: ' + str(ret))


def CheckPresenceSelUniversal():
  'Assert that both sel_universal and sel_ldr exist'
  sel_universal = env.eval('${SEL_UNIVERSAL_%arch%}')
  if not os.path.exists(sel_universal):
    Log.Fatal('Could not find sel_universal [%s]', sel_universal)
  sel_ldr = env.eval('${SEL_LDR_%arch%}')
  if not os.path.exists(sel_ldr):
    Log.Fatal('Could not find sel_ldr [%s]', sel_ldr)


def MakeSelUniversalScriptForLLC(infile, outfile, flags):
  script = []
  script.append('readonly_file myfile %s' % infile)
  for f in flags:
    script.append('rpc AddArg s("%s") *' % f)
  script.append('rpc Translate h(myfile) * h() i()')
  script.append('set_variable out_handle ${result0}')
  script.append('set_variable out_size ${result1}')
  script.append('map_shmem ${out_handle} addr')
  script.append('save_to_file %s ${addr} 0 ${out_size}' % outfile)
  script.append('')
  return '\n'.join(script)


def RunLLCSRPC():
  CheckPresenceSelUniversal()
  infile = env.get("input")
  outfile = env.get("output")
  flags = shell.split(env.get("LLC_FLAGS"))
  script = MakeSelUniversalScriptForLLC(infile, outfile, flags)

  RunWithLog('${SEL_UNIVERSAL_PREFIX} "${SEL_UNIVERSAL_%arch%}" ' +
             '${SEL_UNIVERSAL_FLAGS} -- "${LLC_SRPC_%arch%}"',
             stdin=script, silent = True)


def FindLib(name, searchdirs):
  """Returns the full pathname for the library named 'name'.
     For example, name might be "c" or "m".
     Returns None if the library is not found.
  """
  matches = [ 'lib' + name + '.a', 'lib' + name + '.so' ]
  for curdir in searchdirs:
    for m in matches:
      guess = os.path.join(curdir, m)
      if os.path.exists(guess):
        return guess
  return None

# Given linker arguments (including -L, -l, and filenames),
# returns the list of files which are pulled by the linker.
def LinkerFiles(args):
  searchdirs = []
  for f in args:
    if f.startswith('-L'):
      searchdirs.append(f[2:])

  ret = []
  for f in args:
    if f.startswith('-L'):
      continue
    elif f.startswith('-l'):
      libpath = FindLib(f[2:], searchdirs)
      if libpath is None:
        Log.Fatal("Unable to find library '%s'", f)
      ret.append(libpath)
      continue
    else:
      if not os.path.exists(f):
        Log.Fatal("Unable to open '%s'", f)
      ret.append(f)
  return ret

def MakeSelUniversalScriptForFile(filename):
  """ Return sel_universal script text for sending a commandline argument
      representing an input file to LD.nexe. """
  script = []
  basename = os.path.basename(filename)
  # A nice name for making a sel_universal variable.
  # Hopefully this does not clash...
  nicename = basename.replace('.','_').replace('-','_')
  script.append('echo "adding %s"' % basename)
  script.append('readonly_file %s %s' % (nicename, filename))
  script.append('rpc AddFile s("%s") h(%s) *' % (basename, nicename))
  script.append('rpc AddArg s("%s") *' % basename)
  return script

def MakeSelUniversalScriptForLD(ld_flags,
                                ld_script,
                                main_input,
                                files,
                                outfile):
  """ Return sel_universal script text for invoking LD.nexe with the
      given ld_flags, main_input (which is treated specially), and
      other input files. If ld_script is part of the ld_flags, it must be
      treated it as input file. The output will be written to outfile.  """
  script = []

  # Go through all the arguments and add them.
  # Based on the format of RUN_LD, the order of arguments is:
  # ld_flags, then input files (which are more sensitive to order).
  # Omit the "-o" for output so that it will use "a.out" internally.
  # We will take the fd from "a.out" and write it to the proper -o filename.
  for flag in ld_flags:
    if flag == ld_script:
      # Virtualize the file.
      script += MakeSelUniversalScriptForFile(ld_script)
    else:
      script.append('rpc AddArg s("%s") *' % flag)
    script.append('')

  # We need to virtualize these files.
  for f in files:
    if f == main_input:
      # Reload the temporary main_input object file into a new shmem region.
      basename = os.path.basename(f)
      script.append('file_size %s in_size' % f)
      script.append('shmem in_file in_addr ${in_size}')
      script.append('load_from_file %s ${in_addr} 0 ${in_size}' % f)
      script.append('rpc AddFileWithSize s("%s") h(in_file) i(${in_size}) *' %
                    basename)
      script.append('rpc AddArg s("%s") *' % basename)
    else:
      script += MakeSelUniversalScriptForFile(f)
    script.append('')

  script.append('rpc Link * h() i()')
  script.append('set_variable out_file ${result0}')
  script.append('set_variable out_size ${result1}')
  script.append('map_shmem ${out_file} out_addr')
  script.append('save_to_file %s ${out_addr} 0 ${out_size}' % outfile)
  script.append('echo "ld complete"')
  script.append('')
  return '\n'.join(script)

def RunLDSRPC():
  CheckPresenceSelUniversal()
  # The "main" input file is the application's combined object file.
  main_input = env.get('inputs')
  all_inputs = env.get('LD_INPUTS')
  outfile = env.get('output')

  assert(len(shell.split(main_input)) == 1)
  files = LinkerFiles(shell.split(all_inputs))
  ld_flags = shell.split(env.get('LD_FLAGS') + ' ' + env.get('LD_BFD_FLAGS'))
  ld_script = env.eval('${LD_SCRIPT_%arch%}')

  script = MakeSelUniversalScriptForLD(ld_flags,
                                       ld_script,
                                       main_input,
                                       files,
                                       outfile)

  RunWithLog('"${SEL_UNIVERSAL_%arch%}" ${SEL_UNIVERSAL_FLAGS} -- ' +
             '"${LD_SRPC_%arch%}"', stdin=script, silent = True)


######################################################################
# Bitcode Link Wrap Symbols Hack
######################################################################

def WrapSymbols(symbols, infile, outfile):
  assert(FileType(infile) == 'll')
  Log.Info('Wrapping symbols: ' + ' '.join(symbols))

  try:
    fp = open(infile, 'r')
  except Exception:
    print "Failed to open input file " + infile
    NiceExit(1)

  result = []
  for line in fp.readlines():
    for s in symbols:
      # Relabel the real function
      if line.startswith('define'):
        line = line.replace('@' + s + '(', '@__real_' + s + '(')

      # Remove declarations of __real_xyz symbol.
      # Because we are actually defining it now, leaving the declaration around
      # would cause an error. (bitcode should have either a define or declare,
      # not both).
      if line.startswith('declare') and '__real_' in line:
        line = ''

      # Relabel the wrapper to the original name
      line = line.replace('@__wrap_' + s + '(', '@' + s + '(')
    result.append(line)
  fp.close()

  fp = open(outfile, 'w')
  for line in result:
    fp.write(line)
  fp.close()

######################################################################
# Invocation
######################################################################

if __name__ == "__main__":
  # uncomment to force noisy output
  # sys.argv = [sys.argv[0], '--pnacl-driver-verbose'] + sys.argv[1:]
  NiceExit(main(sys.argv))
