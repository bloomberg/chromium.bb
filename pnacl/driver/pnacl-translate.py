#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  pnacl/build.sh driver
# in order for them to take effect in the scons build.  This command
# updates the copy in the toolchain/ tree.
#

import driver_tools
import filetype
import ldtools
import multiprocessing
import os
import pathtools
import shutil
from driver_env import env
from driver_log import Log
from driver_temps import TempFiles

import re
import subprocess

EXTRA_ENV = {
  'PIC'           : '0',

  # Determine if we should build nexes compatible with the IRT
  'USE_IRT' : '1',

  # Allow zero-cost C++ exception handling in the pexe, which is not
  # supported by PNaCl's stable ABI.
  'ALLOW_ZEROCOST_CXX_EH' : '0',

  # Use the IRT shim by default. This can be disabled with an explicit
  # flag (--noirtshim) or via -nostdlib.
  'USE_IRT_SHIM'  : '1',

  # To simulate the sandboxed translator better and avoid user surprises,
  # reject LLVM bitcode (non-finalized) by default, accepting only PNaCl
  # (finalized) bitcode. --allow-llvm-bitcode-input has to be passed
  # explicitly to override this.
  'ALLOW_LLVM_BITCODE_INPUT': '0',

  # Flags for pnacl-nativeld
  'LD_FLAGS': '-static',

  'USE_STDLIB'     : '1',
  'USE_DEFAULTLIBS': '1',
  'FAST_TRANSLATION': '0',

  'INPUTS'        : '',
  'OUTPUT'        : '',
  'OUTPUT_TYPE'   : '',

  # Library Strings
  'LD_ARGS' : '${USE_STDLIB ? ${LD_ARGS_normal} : ${LD_ARGS_nostdlib}}',

  # Note: we always requires a shim now, but the dummy shim is not doing
  # anything useful.
  # libpnacl_irt_shim.a is generated during the SDK packaging not
  # during the toolchain build and there are hacks in pnacl/driver/ldtools.py
  # and pnacl/driver/pnacl-nativeld.py that will fall back to
  # libpnacl_irt_shim_dummy.a if libpnacl_irt_shim.a does not exist.
  'LD_ARGS_IRT_SHIM': '-l:libpnacl_irt_shim.a',
  'LD_ARGS_IRT_SHIM_DUMMY': '-l:libpnacl_irt_shim_dummy.a',
  # In addition to specifying the entry point, we also specify an undefined
  # reference to _start, which is called by the shim's entry function,
  # __pnacl_wrapper_start. _start normally comes from libnacl and will be in
  # the pexe, however for the IRT it comes from irt_entry.c and when linking it
  # using native object files, this reference is required to make sure it gets
  # pulled in from the archive.
  'LD_ARGS_ENTRY': '--entry=__pnacl_start --undefined=_start',

  'CRTBEGIN': '${ALLOW_ZEROCOST_CXX_EH ? -l:crtbegin_for_eh.o : -l:crtbegin.o}',
  'CRTEND': '-l:crtend.o',

  'LD_ARGS_nostdlib': '-nostdlib ${ld_inputs}',

  # These are just the dependencies in the native link.
  'LD_ARGS_normal':
    '${CRTBEGIN} ${ld_inputs} ' +
    '${USE_IRT_SHIM ? ${LD_ARGS_IRT_SHIM} : ${LD_ARGS_IRT_SHIM_DUMMY}} ' +
    '--start-group ' +
    '${USE_DEFAULTLIBS ? ${DEFAULTLIBS}} ' +
    '--end-group ' +
    '${CRTEND}',

  'DEFAULTLIBS': '${ALLOW_ZEROCOST_CXX_EH ? -l:libgcc_eh.a} ' +
                 '-l:libgcc.a -l:libcrt_platform.a ',

  'TRIPLE'      : '${TRIPLE_%ARCH%}',
  'TRIPLE_ARM'  : 'armv7a-none-nacl-gnueabihf',
  'TRIPLE_X8632': 'i686-none-nacl-gnu',
  'TRIPLE_X8664': 'x86_64-none-nacl-gnu',
  'TRIPLE_MIPS32': 'mipsel-none-nacl-gnu',
  'TRIPLE_LINUX_X8632': 'i686-linux-gnu',

  'LLC_FLAGS_COMMON': '${PIC ? -relocation-model=pic} ' +
                      #  -force-tls-non-pic makes the code generator (llc)
                      # do the work that would otherwise be done by
                      # linker rewrites which are quite messy in the nacl
                      # case and hence have not been implemented in gold
                      '${PIC ? -force-tls-non-pic} ',


  'LLC_FLAGS_ARM'    :
    ('-arm-reserve-r9 -sfi-disable-cp ' +
     '-sfi-load -sfi-store -sfi-stack -sfi-branch -sfi-data ' +
     '-no-inline-jumptables -float-abi=hard -mattr=+neon'),

  'LLC_FLAGS_X8632' : '',
  'LLC_FLAGS_X8664' : '',

  'LLC_FLAGS_MIPS32': '-sfi-load -sfi-store -sfi-stack -sfi-branch -sfi-data',

  # When linking against Linux glibc, don't use %gs:0 to read the
  # thread pointer because that's not compatible with glibc's use of
  # %gs.
  'LLC_FLAGS_LINUX_X8632' : '-mtls-use-call',

  # LLC flags which set the target and output type.
  'LLC_FLAGS_TARGET' : '-mtriple=${TRIPLE} -filetype=${outfiletype}',

  # Append additional non-default flags here.
  'LLC_FLAGS_EXTRA' : '${FAST_TRANSLATION ? ${LLC_FLAGS_FAST}} ' +
                      '${#OPT_LEVEL ? -O${OPT_LEVEL}} ' +
                      '${OPT_LEVEL == 0 ? -disable-fp-elim}',

  # Opt level from command line (if any)
  'OPT_LEVEL' : '',

  # faster translation == slower code
  'LLC_FLAGS_FAST' : '${LLC_FLAGS_FAST_%ARCH%}'
                     # This, surprisingly, makes a measurable difference
                     ' -tail-merge-threshold=20',

  'LLC_FLAGS_FAST_X8632': '-O0 ',
  'LLC_FLAGS_FAST_X8664': '-O0 ',
  'LLC_FLAGS_FAST_ARM':   '-O0 ',
  'LLC_FLAGS_FAST_MIPS32': '-fast-isel',
  'LLC_FLAGS_FAST_LINUX_X8632': '-O0',

  'LLC_FLAGS': '${LLC_FLAGS_TARGET} ${LLC_FLAGS_COMMON} ${LLC_FLAGS_%ARCH%} ' +
               '${LLC_FLAGS_EXTRA}',

  # CPU that is representative of baseline feature requirements for NaCl
  # and/or chrome.  We may want to make this more like "-mtune"
  # by specifying both "-mcpu=X" and "-mattr=+feat1,-feat2,...".
  # Note: this may be different from the in-browser translator, which may
  # do auto feature detection based on CPUID, but constrained by what is
  # accepted by NaCl validators.
  'LLC_MCPU'        : '-mcpu=${LLC_MCPU_%ARCH%}',
  'LLC_MCPU_ARM'    : 'cortex-a9',
  'LLC_MCPU_X8632'  : 'pentium4',
  'LLC_MCPU_X8664'  : 'core2',
  'LLC_MCPU_MIPS32' : 'mips32r2',
  'LLC_MCPU_LINUX_X8632' : '${LLC_MCPU_X8632}',

  # Note: this is only used in the unsandboxed case
  'RUN_LLC'       : '${LLVM_PNACL_LLC} ${LLC_FLAGS} ${LLC_MCPU} '
                    '${input} -o ${output} ',
  # Whether to stream the bitcode from a single FD in unsandboxed mode
  # (otherwise it will use concurrent file reads when using multithreaded module
  # splitting)
  'STREAM_BITCODE' : '1',
  # Rate in bits/sec to stream the bitcode from sel_universal over SRPC
  # for testing. Defaults to 1Gbps (effectively unlimited).
  'BITCODE_STREAM_RATE' : '1000000000',
  # Default to 0, which means unset by the user. In this cases the driver will
  # use up to 4 modules if there are enough cores. If the user overrides,
  # use as many modules as specified (which could be only 1).
  'SPLIT_MODULE' : '0',
}


TranslatorPatterns = [
  ( '-o(.+)',          "env.set('OUTPUT', pathtools.normalize($0))"),
  ( ('-o', '(.+)'),    "env.set('OUTPUT', pathtools.normalize($0))"),

  ( '-S',              "env.set('OUTPUT_TYPE', 's')"), # Stop at .s
  ( '-c',              "env.set('OUTPUT_TYPE', 'o')"), # Stop at .o

  # Expose a very limited set of llc flags.
  ( '(-sfi-.+)',        "env.append('LLC_FLAGS_EXTRA', $0)"),
  ( '(-mtls-use-call)', "env.append('LLC_FLAGS_EXTRA', $0)"),
  # These flags are usually used for linktime dead code/data
  # removal but also help with reloc overflows on ARM
  ( '(-fdata-sections)',     "env.append('LLC_FLAGS_EXTRA', $0)"),
  ( '(-ffunction-sections)', "env.append('LLC_FLAGS_EXTRA', $0)"),
  ( '(--gc-sections)',       "env.append('LD_FLAGS', $0)"),
  ( '(-mattr=.*)', "env.append('LLC_FLAGS_EXTRA', $0)"),
  ( '(-mcpu=.*)', "env.set('LLC_MCPU', '')\n"
                  "env.append('LLC_FLAGS_EXTRA', $0)"),
  ( '(-pnaclabi-verify)', "env.append('LLC_FLAGS_EXTRA', $0)"),
  ( '(-pnaclabi-verify-fatal-errors)', "env.append('LLC_FLAGS_EXTRA', $0)"),
  ( '(-pnaclabi-allow-dev-intrinsics)', "env.append('LLC_FLAGS_EXTRA', $0)"),
  # Allow overriding the -O level.
  ( '-O([0-3])', "env.set('OPT_LEVEL', $0)"),

  # This adds arch specific flags to the llc invocation aimed at
  # improving translation speed at the expense of code quality.
  ( '-translate-fast',  "env.set('FAST_TRANSLATION', '1')"),

  ( '-nostdlib',       "env.set('USE_STDLIB', '0')"),

  # Disables the default libraries.
  # This flag is needed for building libgcc_s.so.
  ( '-nodefaultlibs',  "env.set('USE_DEFAULTLIBS', '0')"),

  ( '--noirt',         "env.set('USE_IRT', '0')\n"
                       "env.append('LD_FLAGS', '--noirt')"),
  ( '--noirtshim',     "env.set('USE_IRT_SHIM', '0')"),
  ( '(--pnacl-nativeld=.+)', "env.append('LD_FLAGS', $0)"),

  # Allowing zero-cost C++ exception handling causes a specific set of
  # native objects to get linked into the nexe.
  ( '--pnacl-allow-zerocost-eh', "env.set('ALLOW_ZEROCOST_CXX_EH', '1')"),
  # TODO(mseaborn): Remove "--pnacl-allow-exceptions", replaced by
  # "--pnacl-allow-zerocost-eh".
  ( '--pnacl-allow-exceptions', "env.set('ALLOW_ZEROCOST_CXX_EH', '1')"),

  ( '--allow-llvm-bitcode-input', "env.set('ALLOW_LLVM_BITCODE_INPUT', '1')"),

  ( '-fPIC',           "env.set('PIC', '1')"),

  ( '(--build-id)',    "env.append('LD_FLAGS', $0)"),
  ( '-bitcode-stream-rate=([0-9]+)', "env.set('BITCODE_STREAM_RATE', $0)"),
  ( '-split-module=([0-9]+)', "env.set('SPLIT_MODULE', $0)"),
  ( '-no-stream-bitcode', "env.set('STREAM_BITCODE', '0')"),

  # Treat general linker flags as inputs so they don't get re-ordered
  ( '-Wl,(.*)',        "env.append('INPUTS', *($0).split(','))"),

  ( '(-.*)',            driver_tools.UnrecognizedOption),
  ( '(.*)',            "env.append('INPUTS', pathtools.normalize($0))"),
]


def main(argv):
  env.update(EXTRA_ENV)
  driver_tools.ParseArgs(argv, TranslatorPatterns)
  driver_tools.GetArch(required = True)

  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')

  if len(inputs) == 0:
    Log.Fatal("No input files")

  if output == '':
    Log.Fatal("Please specify output file with -o")

  # Find the bitcode file on the command line.
  bcfiles = [f for f in inputs
             if not ldtools.IsFlag(f) and
               (filetype.IsPNaClBitcode(f)
                or filetype.IsLLVMBitcode(f)
                or filetype.FileType(f) == 'll')]
  if len(bcfiles) > 1:
    Log.Fatal('Expecting at most 1 bitcode file')
  elif len(bcfiles) == 1:
    bcfile = bcfiles[0]
  else:
    bcfile = None

  if not env.getbool('SPLIT_MODULE'):
    try:
      env.set('SPLIT_MODULE', str(min(4, multiprocessing.cpu_count())))
    except NotImplementedError:
      env.set('SPLIT_MODULE', '2')
  elif int(env.getone('SPLIT_MODULE')) < 1:
    Log.Fatal('Value given for -split-module must be > 0')
  if (env.getbool('ALLOW_LLVM_BITCODE_INPUT') or
      env.getone('ARCH') == 'LINUX_X8632' or
      RequiresNonStandardLDCommandline(inputs, output, 1)[1] or
      env.getbool('USE_EMULATOR')):
    # When llvm input is allowed, the pexe may not be ABI-stable, so do not
    # split it. For now also do not support threading non-SFI baremetal mode.
    # Non-ABI-stable pexes may have symbol naming and visibility issues that the
    # current splitting scheme doesn't account for.
    # If the link would require a non-standard command line, do not split the
    # modules because the sandboxed linker doesn't support that combination.
    # The x86->arm emulator is very flaky when threading is used, so don't
    # do module splitting when using it.
    env.set('SPLIT_MODULE', '1')
  else:
    modules = env.getone('SPLIT_MODULE')
    if modules != '1':
      env.append('LLC_FLAGS_EXTRA', '-split-module=' + modules)
      env.append('LD_FLAGS', '-split-module=' + modules)
    if not env.getbool('SANDBOXED') and env.getbool('STREAM_BITCODE'):
      # Do not set -streaming-bitcode for sandboxed mode, because it is already
      # in the default command line.
      env.append('LLC_FLAGS_EXTRA', '-streaming-bitcode')

  # If there's a bitcode file, translate it now.
  tng = driver_tools.TempNameGen(inputs + bcfiles, output)
  output_type = env.getone('OUTPUT_TYPE')
  if bcfile:
    sfile = None
    if output_type == 's':
      sfile = output

    ofile = None
    if output_type == 'o':
      ofile = output
    elif output_type != 's':
      ofile = tng.TempNameForInput(bcfile, 'o')

    if sfile:
      RunLLC(bcfile, sfile, outfiletype='asm')
      if ofile:
        RunAS(sfile, ofile)
    else:
      RunLLC(bcfile, ofile, outfiletype='obj')
  else:
    ofile = None

  # If we've been told to stop after translation, stop now.
  if output_type in ('o','s'):
    return 0

  # Replace the bitcode file with __BITCODE__ in the input list
  if bcfile:
    inputs = ListReplace(inputs, bcfile, '__BITCODE__')
    env.set('INPUTS', *inputs)
  if int(env.getone('SPLIT_MODULE')) > 1:
    modules = int(env.getone('SPLIT_MODULE'))
    for i in range(1, modules):
      filename = ofile + '.module%d' % i
      TempFiles.add(filename)
      env.append('INPUTS', filename)

  if env.getone('ARCH') == 'LINUX_X8632':
    RunHostLD(ofile, output)
  else:
    RunLD(ofile, output)
  return 0

def RunAS(infile, outfile):
  driver_tools.RunDriver('as', [infile, '-o', outfile])

def ListReplace(items, old, new):
  ret = []
  for k in items:
    if k == old:
      ret.append(new)
    else:
      ret.append(k)
  return ret

def RequiresNonStandardLDCommandline(inputs, infile, num_modules):
  ''' Determine when we must force USE_DEFAULT_CMD_LINE off for running
  the sandboxed LD (if link line is completely non-standard).
  '''
  if len(inputs) != num_modules:
    # There must have been some native objects on the link line.
    # In that case, if we are using the sandboxed translator, we cannot
    # currently allow that with the default commandline (only one input).
    return ('Native link with more than one native object: %s' % str(inputs),
            True)
  if not infile:
    return ('No bitcode input: %s' % str(infile), True)
  if not env.getbool('USE_STDLIB'):
    return ('NOSTDLIB', True)
  if env.getbool('ALLOW_ZEROCOST_CXX_EH'):
    return ('ALLOW_ZEROCOST_CXX_EH', True)
  if not env.getbool('USE_IRT'):
    return ('USE_IRT false when normally true', True)
  if not env.getbool('USE_IRT_SHIM'):
    return ('USE_IRT_SHIM false when normally true', True)
  return (None, False)

def ToggleDefaultCommandlineLD(inputs, infile):
  if env.getbool('USE_DEFAULT_CMD_LINE'):
    reason, non_standard = RequiresNonStandardLDCommandline(
        inputs, infile, int(env.getone('SPLIT_MODULE')))
    if non_standard:
      Log.Info(reason + ' -- not using default SRPC commandline for LD!')
      inputs.append('--pnacl-driver-set-USE_DEFAULT_CMD_LINE=0')


def RequiresNonStandardLLCCommandline():
  extra_flags = env.get('LLC_FLAGS_EXTRA')
  if extra_flags != []:
    reason = 'Has additional llc flags: %s' % extra_flags
    return (reason, True)

  return (None, False)


def UseDefaultCommandlineLLC():
  if not env.getbool('USE_DEFAULT_CMD_LINE'):
    return False
  else:
    reason, non_standard = RequiresNonStandardLLCCommandline()
    if non_standard:
      Log.Info(reason + ' -- not using default SRPC commandline for LLC!')
      return False
    return True

def RunLD(infile, outfile):
  inputs = env.get('INPUTS')
  if infile:
    # Put llc-translated-file at the beginning of the inputs so that it will
    # pull in all needed symbols from any native archives that may also be
    # in the input list. This is in case there are any mixed groups of bitcode
    # and native archives in the link (as is the case with irt_browser_lib)
    inputs.remove('__BITCODE__')
    inputs = ['--llc-translated-file=' + infile] + inputs
  ToggleDefaultCommandlineLD(inputs, infile)
  env.set('ld_inputs', *inputs)
  args = env.get('LD_ARGS') + ['-o', outfile]
  if env.getbool('USE_STDLIB'):
    args += env.get('LD_ARGS_ENTRY')
  args += env.get('LD_FLAGS')
  driver_tools.RunDriver('nativeld', args)

def RunHostLD(infile, outfile):
  driver_tools.Run(['objcopy', '--redefine-sym', '_start=_user_start', infile])
  lib_dir = env.getone('BASE_LIB_NATIVE') + 'linux-x86-32'
  driver_tools.Run(['gcc', '-m32', infile,
                    os.path.join(lib_dir, 'unsandboxed_irt.o'),
                    '-lpthread',
                    '-lrt',  # For clock_gettime()
                    '-o', outfile])

def RunLLC(infile, outfile, outfiletype):
  env.push()
  env.setmany(input=infile, output=outfile, outfiletype=outfiletype)
  if env.getbool('SANDBOXED'):
    RunLLCSandboxed()
    env.pop()
  else:
    args = ["${RUN_LLC}"]
    if filetype.IsPNaClBitcode(infile):
      args.append("-bitcode-format=pnacl")
    elif filetype.IsLLVMBitcode(infile):
      if not env.getbool('ALLOW_LLVM_BITCODE_INPUT'):
        Log.Fatal('Translator expects finalized PNaCl bitcode. '
                  'Pass --allow-llvm-bitcode-input to override.')
    driver_tools.Run(' '.join(args))
    env.pop()
  return 0

def RunLLCSandboxed():
  driver_tools.CheckTranslatorPrerequisites()
  infile = env.getone('input')
  outfile = env.getone('output')
  if not filetype.IsPNaClBitcode(infile):
    Log.Fatal('Input to sandboxed translator must be PNaCl bitcode')
  script = MakeSelUniversalScriptForLLC(infile, outfile)
  command = ('${SEL_UNIVERSAL_PREFIX} ${SEL_UNIVERSAL} ${SEL_UNIVERSAL_FLAGS} '
    '-- ${LLC_SB}')
  driver_tools.Run(command,
                   stdin_contents=script,
                   # stdout/stderr will be automatically dumped
                   # upon failure
                   redirect_stderr=subprocess.PIPE,
                   redirect_stdout=subprocess.PIPE)

def BuildOverrideLLCCommandLine():
  extra_flags = env.get('LLC_FLAGS_EXTRA')
  # The mcpu is not part of the default flags, so append that too.
  mcpu = env.getone('LLC_MCPU')
  if mcpu:
    extra_flags.append(mcpu)
  # command_line is a NUL (\x00) terminated sequence.
  kTerminator = '\0'
  command_line = kTerminator.join(extra_flags) + kTerminator
  command_line_escaped = command_line.replace(kTerminator, '\\x00')
  return len(command_line), command_line_escaped

def MakeSelUniversalScriptForLLC(infile, outfile):
  script = []
  script.append('readwrite_file objfile %s' % outfile)
  modules = int(env.getone('SPLIT_MODULE'))
  if modules > 1:
    script.extend(['readwrite_file objfile%d %s.module%d' % (m, outfile, m)
                   for m in range(1, modules)])
  stream_rate = int(env.getraw('BITCODE_STREAM_RATE'))
  assert stream_rate != 0
  if UseDefaultCommandlineLLC():
    script.append('rpc StreamInit h(objfile) * s()')
  else:
    cmdline_len, cmdline_escaped = BuildOverrideLLCCommandLine()
    assert modules in range(1, 17)
    script.append('rpc StreamInitWithSplit i(%d) h(objfile) ' % modules +
                  ' '.join(['h(objfile%d)' % m for m in range(1, modules)] +
                           ['h(invalid)' for x in range(modules, 16)]) +
                  ' C(%d,%s) * s()' % (cmdline_len, cmdline_escaped))
  # specify filename, chunk size and rate in bits/s
  script.append('stream_file %s %s %s' % (infile, 64 * 1024, stream_rate))
  script.append('rpc StreamEnd * i() s() s() s()')
  script.append('echo "pnacl-llc complete"')
  script.append('')
  return '\n'.join(script)

def get_help(argv):
  return """
PNaCl bitcode to native code translator.

Usage: pnacl-translate [options] -arch <arch> <input> -o <output>

  <input>                 Input file (bitcode).
  -arch <arch>            Translate to <arch> (i686, x86-64, armv7).
                          Note: <arch> is a generic arch specifier.  This
                          generates code assuming certain baseline CPU features,
                          required by NaCl or Chrome. For finer control of
                          tuning and features, see -mattr, and -mcpu.
  -o <output>             Output file.

  The default output file type is .nexe, which assumes that the input file
  type is .pexe.  Native object files and assembly can also be generated
  with the -S and -c commandline flags.

ADVANCED OPTIONS:
  -mattr=<+feat1,-feat2>  Toggle specific cpu features on and off.
  -mcpu=<cpu-name>        Target a specific cpu type.  Tunes code as well as
                          turns cpu features on and off.
  -S                      Generate native assembly only.
  -c                      Generate native object file only.
  --pnacl-sb              Use the translator which runs inside the NaCl sandbox.
  -O[0-3]                 Change translation-time optimization level.
"""
