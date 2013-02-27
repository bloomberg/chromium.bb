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
import pathtools
import shutil
from driver_env import env
from driver_log import Log, TempFiles

import re
import subprocess

EXTRA_ENV = {
  'PIC'           : '0',

  # Determine if we should build nexes compatible with the IRT
  'USE_IRT' : '1',

  # Use the IRT shim by default. This can be disabled with an explicit
  # flag (--noirtshim) or via -nostdlib.
  'USE_IRT_SHIM'  : '${!SHARED ? 1 : 0}',
  # Experimental mode exploring newlib as a shared library
  'NEWLIB_SHARED_EXPERIMENT': '0',

  # Flags for pnacl-nativeld
  'LD_FLAGS': '${STATIC ? -static} ${SHARED ? -shared}',

  'STATIC'         : '0',
  'SHARED'         : '0',
  'STDLIB'         : '1',
  'USE_DEFAULTLIBS': '1',
  'FAST_TRANSLATION': '0',

  'INPUTS'        : '',
  'OUTPUT'        : '',
  'OUTPUT_TYPE'   : '',

  # Library Strings
  'LD_ARGS' : '${STDLIB ? ${LD_ARGS_normal} : ${LD_ARGS_nostdlib}}',

  # Note: we always requires a shim now, but the dummy shim is not doing
  # anything useful.
  # libpnacl_irt_shim.a is generated during the SDK packaging not
  # during the toolchain build and there are hacks in pnacl/driver/ldtools.py
  # and pnacl/driver/pnacl-nativeld.py that will fall back to
  # libpnacl_irt_shim_dummy.a if libpnacl_irt_shim.a does not exist.
  'LD_ARGS_IRT_SHIM': '-l:libpnacl_irt_shim.a',
  'LD_ARGS_IRT_SHIM_DUMMY': '-l:libpnacl_irt_shim_dummy.a',

  'LD_ARGS_ENTRY': '--entry=__pnacl_start',

  'CRTBEGIN' : '${SHARED ? -l:crtbeginS.o : -l:crtbegin.o}',
  'CRTEND'   : '${SHARED ? -l:crtendS.o : -l:crtend.o}',
  # static and dynamic newlib images link against the static libgcc_eh
  'LIBGCC_EH': '${STATIC || NEWLIB_SHARED_EXPERIMENT && !SHARED ? ' +
               '-l:libgcc_eh.a : ' +
               # NOTE: libgcc_s.so drags in "glibc.so"
               # TODO(robertm): provide a "better" libgcc_s.so
               ' ${NEWLIB_SHARED_EXPERIMENT ? : -l:libgcc_s.so.1}}',

  # List of user requested libraries (according to bitcode metadata).
  'NEEDED_LIBRARIES': '',   # Set by ApplyBitcodeConfig.

  'LD_ARGS_nostdlib': '-nostdlib ${ld_inputs} ${NEEDED_LIBRARIES}',

  # These are just the dependencies in the native link.
  'LD_ARGS_normal':
    '${CRTBEGIN} ${ld_inputs} ' +
    '${USE_IRT_SHIM ? ${LD_ARGS_IRT_SHIM} : ${LD_ARGS_IRT_SHIM_DUMMY}} ' +
    '${NEEDED_LIBRARIES} ' +
    '${STATIC ? --start-group} ' +
    '${USE_DEFAULTLIBS ? ${DEFAULTLIBS}} ' +
    '${STATIC ? --end-group} ' +
    '${CRTEND}',

  # TODO(pdox): To simplify translation, reduce from 3 to 2 cases.
  # BUG= http://code.google.com/p/nativeclient/issues/detail?id=2423
  'DEFAULTLIBS':
    '${LIBGCC_EH} -l:libgcc.a ${MISC_LIBS}',

  'MISC_LIBS':
    # TODO(pdox):
    # Move libcrt_platform into the __pnacl namespace,
    # with stubs to access it from newlib.
    '${LIBMODE_NEWLIB ? -l:libcrt_platform.a} ',

  # Determine whether or not to use bitcode metadata to generate .so stubs
  # for the final link.
  'USE_META': '0',

  'TRIPLE'      : '${TRIPLE_%ARCH%}',
  'TRIPLE_ARM'  : 'armv7a-none-nacl-gnueabi',
  'TRIPLE_X8632': 'i686-none-nacl-gnu',
  'TRIPLE_X8664': 'x86_64-none-nacl-gnu',
  'TRIPLE_MIPS32': 'mipsel-none-nacl-gnu',

  'LLC_FLAGS_COMMON': '${PIC ? -relocation-model=pic} ' +
                      #  -force-tls-non-pic makes the code generator (llc)
                      # do the work that would otherwise be done by
                      # linker rewrites which are quite messy in the nacl
                      # case and hence have not been implemented in gold
                      '${PIC && !SHARED ? -force-tls-non-pic} ' +
                      # this translates the pexe one function at a time
                      # which is also what the streaming translation does
                      '-reduce-memory-footprint',


  'LLC_FLAGS_ARM'    :
    ('-arm-reserve-r9 -sfi-disable-cp ' +
     '-sfi-load -sfi-store -sfi-stack -sfi-branch -sfi-data ' +
     '-no-inline-jumptables -float-abi=hard'),

  'LLC_FLAGS_X8632' : '',
  'LLC_FLAGS_X8664' : '',

  'LLC_FLAGS_MIPS32': '-sfi-load -sfi-store -sfi-stack -sfi-branch -sfi-data',

  # LLC flags which set the target and output type.
  # These are handled separately by libLTO.
  'LLC_FLAGS_TARGET' : '-mcpu=${LLC_MCPU} ' +
                       '-mtriple=${TRIPLE} ' +
                       '-filetype=${filetype}',
  # Additional non-default flags go here.
  'LLC_FLAGS_EXTRA' : '',

  # Opt level from command line (if any)
  'OPT_LEVEL' : '',

  # slower translation == faster code
  'LLC_FLAGS_SLOW':
  # Due to a quadratic algorithm used for tail merging
  # capping it at 50 helps speed up translation
                     '-tail-merge-threshold=50',

  # faster translation == slower code
  'LLC_FLAGS_FAST' : '${LLC_FLAGS_FAST_%ARCH%}',

  'LLC_FLAGS_FAST_X8632':
                          '-O0 ' +
  # This, surprisingly, makes a measurable difference
                          '-tail-merge-threshold=20',
  'LLC_FLAGS_FAST_X8664':
                          '-O0 ' +
                          '-tail-merge-threshold=20',
  'LLC_FLAGS_FAST_ARM':
  # due to slow turn around times ARM settings have not been explored in depth
                          '-O0 ' +
                          '-tail-merge-threshold=20',
  'LLC_FLAGS_FAST_MIPS32': '-fast-isel -tail-merge-threshold=20',

  'LLC_FLAGS': '${LLC_FLAGS_TARGET} ' +
               '${LLC_FLAGS_COMMON} ' +
               '${LLC_FLAGS_%ARCH%} ' +
               '${FAST_TRANSLATION ? ${LLC_FLAGS_FAST} : ${LLC_FLAGS_SLOW}} ' +
               '${LLC_FLAGS_EXTRA} ' +
               '${#OPT_LEVEL ? -O${OPT_LEVEL}} ' +
               '${OPT_LEVEL == 0 ? -disable-fp-elim}',

  # CPU that is representative of baseline feature requirements for NaCl
  # and/or chrome.  We may want to make this more like "-mtune"
  # by specifying both "-mcpu=X" and "-mattr=+feat1,-feat2,...".
  # Note: this may be different from the in-browser translator, which may
  # do auto feature detection based on CPUID, but constrained by what is
  # accepted by NaCl validators.
  'LLC_MCPU'        : '${LLC_MCPU_%ARCH%}',
  'LLC_MCPU_ARM'    : 'cortex-a8',
  'LLC_MCPU_X8632'  : 'pentium4',
  'LLC_MCPU_X8664'  : 'core2',
  'LLC_MCPU_MIPS32' : 'mips32r2',

  # Note: this is only used in the unsandboxed case
  'RUN_LLC'       : '${LLVM_LLC} ${LLC_FLAGS} ${input} -o ${output} ' +
                    '-metadata-text ${output}.meta',
  # Rate in bits/sec to stream the bitcode from sel_universal over SRPC
  # for testing. Defaults to 1Gbps (effectively unlimited).
  'BITCODE_STREAM_RATE' : '1000000000',
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
  ( '-mcpu=(.*)', "env.set('LLC_MCPU', $0)"),
  # Allow overriding the -O level.
  ( '-O([0-3])', "env.set('OPT_LEVEL', $0)"),

  # This adds arch specific flags to the llc invocation aimed at
  # improving translation speed at the expense of code quality.
  ( '-translate-fast',  "env.set('FAST_TRANSLATION', '1')"),

  # If translating a .pexe which was linked statically against
  # glibc, then you must do pnacl-translate -static. This will
  # be removed once GLibC is actually statically linked.
  ( '-static',         "env.set('STATIC', '1')"),
  ( '-shared',         "env.set('SHARED', '1')"),
  ( '-nostdlib',       "env.set('STDLIB', '0')"),

  # Disables the default libraries.
  # This flag is needed for building libgcc_s.so.
  ( '-nodefaultlibs',  "env.set('USE_DEFAULTLIBS', '0')"),

  ( '--noirt',         "env.set('USE_IRT', '0')\n"
                       "env.append('LD_FLAGS', '--noirt')"),
  ( '--noirtshim',     "env.set('USE_IRT_SHIM', '0')"),
  ( '--cc-rewrite',    "env.set('USE_IRT_SHIM', '0')\n"
                       "env.append('LLC_FLAGS_EXTRA', '--nacl-cc-rewrite')"),
  ( '--newlib-shared-experiment',  "env.set('NEWLIB_SHARED_EXPERIMENT', '1')"),

  # Toggle the use of ELF-stubs / bitcode metadata, which represent real .so
  # files in the final native link.
  # There may be cases where this will not work (e.g., when the final link
  # includes native .o files, where its imports / exports were not known
  # at bitcode link time, and not added to the bitcode metadata).
  ( '-usemeta',         "env.set('USE_META', '1')"),
  ( '-nousemeta',       "env.set('USE_META', '0')"),

  ( '-rpath-link=(.+)', "env.append('LD_FLAGS', '-L'+$0)"),

  ( '-fPIC',           "env.set('PIC', '1')"),

  ( '-Wl,(.*)',        "env.append('LD_FLAGS', *($0).split(','))"),
  ( '-bitcode-stream-rate=([0-9]+)', "env.set('BITCODE_STREAM_RATE', $0)"),

  ( '(-.*)',            driver_tools.UnrecognizedOption),

  ( '(.*)',            "env.append('INPUTS', pathtools.normalize($0))"),
]


def main(argv):
  env.update(EXTRA_ENV)
  driver_tools.ParseArgs(argv, TranslatorPatterns)
  if env.getbool('SHARED'):
    env.set('PIC', '1')

  if env.getbool('SHARED') and env.getbool('STATIC'):
    Log.Fatal('Cannot mix -static and -shared')

  driver_tools.GetArch(required = True)

  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')

  if len(inputs) == 0:
    Log.Fatal("No input files")

  if output == '':
    Log.Fatal("Please specify output file with -o")

  # Find the bitcode file on the command line.
  bcfiles = [f for f in inputs
             if driver_tools.IsBitcode(f) or driver_tools.FileType(f) == 'll']
  if len(bcfiles) > 1:
    Log.Fatal('Expecting at most 1 bitcode file')
  elif len(bcfiles) == 1:
    bcfile = bcfiles[0]
  else:
    bcfile = None

  # If there's a bitcode file, translate it now.
  tng = driver_tools.TempNameGen(inputs + bcfiles, output)
  output_type = env.getone('OUTPUT_TYPE')
  metadata = None
  if bcfile:
    sfile = None
    if output_type == 's':
      sfile = output
    elif env.getbool('FORCE_INTERMEDIATE_S'):
      sfile = tng.TempNameForInput(bcfile, 's')

    ofile = None
    if output_type == 'o':
      ofile = output
    elif output_type != 's':
      ofile = tng.TempNameForInput(bcfile, 'o')

    if sfile:
      RunLLC(bcfile, sfile, filetype='asm')
      if ofile:
        RunAS(sfile, ofile)
    else:
      RunLLC(bcfile, ofile, filetype='obj')
  else:
    ofile = None

  # If we've been told to stop after translation, stop now.
  if output_type in ('o','s'):
    return 0

  # Replace the bitcode file with __BITCODE__ in the input list
  if bcfile:
    inputs = ListReplace(inputs, bcfile, '__BITCODE__')
    env.set('INPUTS', *inputs)

  # Get bitcode type and metadata
  if bcfile:
    bctype = driver_tools.FileType(bcfile)
    # sanity checking
    if bctype == 'pso':
      assert not env.getbool('STATIC')
      assert env.getbool('SHARED')
      assert env.getbool('PIC')
      assert env.get('ARCH') != 'arm', "no glibc support for arm yet"
    elif bctype == 'pexe':
      if env.getbool('LIBMODE_GLIBC'):   # this is our proxy for dynamic images
        assert not env.getbool('STATIC')
        assert not env.getbool('SHARED')
        assert env.get('ARCH') != 'arm', "no glibc support for arm yet"
        # for the dynamic case we require non-pic because we do not
        # have the necessary tls rewrites in gold
        assert not env.getbool('PIC')
      else:
        assert env.getbool('LIBMODE_NEWLIB')
        assert not env.getbool('SHARED')
        # for static images we tolerate both PIC and non-PIC
    else:
      assert False

    metadata = driver_tools.GetBitcodeMetadata(bcfile)

  # Determine the output type, in this order of precedence:
  # 1) Output type can be specified on command-line (-S, -c, -shared, -static)
  # 2) If bitcode file given, use its output type. (pso->so, pexe->nexe, po->o)
  # 3) Otherwise, assume nexe output.
  if env.getbool('SHARED'):
    output_type = 'so'
  elif env.getbool('STATIC'):
    output_type = 'nexe'
  elif bcfile:
    DefaultOutputTypes = {
      'pso' : 'so',
      'pexe': 'nexe',
      'po'  : 'o',
    }
    output_type = DefaultOutputTypes[bctype]
  else:
    output_type = 'nexe'

  # If the bitcode is of type "object", no linking is required.
  if output_type == 'o':
    # Copy ofile to output
    Log.Info('Copying %s to %s' % (ofile, output))
    shutil.copy(pathtools.tosys(ofile), pathtools.tosys(output))
    return 0

  if bcfile:
    ApplyBitcodeConfig(metadata, bctype)

  # NOTE: we intentionally delay setting STATIC here to give user choices
  #       preference but we should think about dropping the support for
  #       the -static, -shared flags in the translator and have everything
  #       be determined by bctype
  if metadata is None:
    env.set('STATIC', '1')
  elif len(metadata['NeedsLibrary']) == 0 and not env.getbool('SHARED'):
    env.set('STATIC', '1')

  assert output_type in ('so','nexe')
  RunLD(ofile, output)
  return 0

def ApplyBitcodeConfig(metadata, bctype):
  # Read the bitcode metadata to extract library
  # dependencies and SOName.
  # For now, we use LD_FLAGS to convey the information.
  # However, if the metadata becomes richer we will need another mechanism.
  # TODO(jvoung): at least grep out the SRPC output from LLC and transmit
  # that directly to LD to avoid issues with mismatching delimiters.
  for needed in metadata['NeedsLibrary']:
    env.append('LD_FLAGS', '--add-extra-dt-needed=' + needed)
  if bctype == 'pso':
    soname = metadata['SOName']
    if soname:
      env.append('LD_FLAGS', '-soname=' + soname)

  # For the current LD final linker, native libraries still need to be
  # linked directly, since --add-extra-dt-needed isn't enough.
  # BUG= http://code.google.com/p/nativeclient/issues/detail?id=2451
  # For the under-construction gold final linker, it expects to have
  # the needed libraries on the commandline as "-llib1 -llib2", which actually
  # refer to the stub metadata file.
  for needed in metadata['NeedsLibrary']:
    # Specify the ld-nacl-${arch}.so as the --dynamic-linker to set PT_INTERP.
    # Also, the original libc.so linker script had it listed as --as-needed,
    # so let's do that.
    if needed.startswith('ld-nacl-'):
      env.append('NEEDED_LIBRARIES', '--dynamic-linker=' + needed)
      env.append('NEEDED_LIBRARIES',
                 '--as-needed',
                 # We normally would have a symlink between
                 # ld-2.9 <-> ld-nacl-${arch}.so, but our native lib directory
                 # has no symlinks (to make windows + cygwin happy).
                 # Link to ld-2.9.so instead for now.
                 '-l:ld-2.9.so',
                 '--no-as-needed')
    else:
      env.append('NEEDED_LIBRARIES', '-l:' + needed)
    # libc and libpthread may need the nonshared components too.
    # Normally these are enclosed in --start-group and --end-group...
    if not env.getbool('NEWLIB_SHARED_EXPERIMENT'):
      if needed.startswith('libc.so'):
        env.append('NEEDED_LIBRARIES', '-l:libc_nonshared.a')
      elif needed.startswith('libpthread.so'):
        env.append('NEEDED_LIBRARIES', '-l:libpthread_nonshared.a')


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

def RequiresNonStandardLDCommandline(inputs, infile):
  ''' Determine when we must force USE_DEFAULT_CMD_LINE off for running
  the sandboxed LD (if link line is completely non-standard).
  '''
  if len(inputs) > 1:
    # There must have been some native objects on the link line.
    # In that case, if we are using the sandboxed translator, we cannot
    # currently allow that with the default commandline (only one input).
    return ('Native link with more than one native object: %s' % str(inputs),
            True)
  if not infile:
    return ('No bitcode input: %s' % str(infile), True)
  if not env.getbool('STDLIB'):
    return ('NOSTDLIB', True)
  if not env.getbool('USE_IRT'):
    return ('USE_IRT false when normally true', True)
  if (not env.getbool('SHARED') and
      not env.getbool('USE_IRT_SHIM')):
    return ('USE_IRT_SHIM false when normally true', True)
  return (None, False)

def ToggleDefaultCommandlineLD(inputs, infile):
  if env.getbool('USE_DEFAULT_CMD_LINE'):
    reason, non_standard = RequiresNonStandardLDCommandline(inputs, infile)
    if non_standard:
      Log.Info(reason + ' -- not using default SRPC commandline for LD!')
      inputs.append('--pnacl-driver-set-USE_DEFAULT_CMD_LINE=0')


def RequiresNonStandardLLCCommandline():
  if env.getbool('FAST_TRANSLATION'):
    return ('FAST_TRANSLATION', True)

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
    inputs = ListReplace(inputs,
                         '__BITCODE__',
                         '--llc-translated-file=' + infile)
  ToggleDefaultCommandlineLD(inputs, infile)
  env.set('ld_inputs', *inputs)
  args = env.get('LD_ARGS') + ['-o', outfile]
  if not env.getbool('SHARED') and env.getbool('STDLIB'):
    args += env.get('LD_ARGS_ENTRY')
  args += env.get('LD_FLAGS')
  # If there is bitcode, there is also a metadata file.
  if infile and env.getbool('USE_META'):
    args += ['--metadata', '%s.meta' % infile]
  driver_tools.RunDriver('nativeld', args)

def RunLLC(infile, outfile, filetype):
  env.push()
  env.setmany(input=infile, output=outfile, filetype=filetype)
  if env.getbool('SANDBOXED'):
    is_shared, soname, needed = RunLLCSandboxed()
    env.pop()
    # soname and dt_needed libs are returned from LLC and passed to LD
    driver_tools.SetBitcodeMetadata(infile, is_shared, soname, needed)
  else:
    driver_tools.Run("${RUN_LLC}")
    # As a side effect, this creates a temporary file
    if not env.getbool('SAVE_TEMPS'):
      TempFiles.add(outfile + '.meta')
    env.pop()
  return 0

def RunLLCSandboxed():
  driver_tools.CheckTranslatorPrerequisites()
  infile = env.getone('input')
  outfile = env.getone('output')
  flags = env.get('LLC_FLAGS')
  if not driver_tools.IsBitcode(infile):
    Log.Fatal('Input to sandboxed translator must be bitcode')
  script = MakeSelUniversalScriptForLLC(infile, outfile, flags)
  command = ('${SEL_UNIVERSAL_PREFIX} ${SEL_UNIVERSAL} ${SEL_UNIVERSAL_FLAGS} '
    '-- ${LLC_SB}')
  _, stdout, _  = driver_tools.Run(command,
                                stdin_contents=script,
                                # stdout/stderr will be automatically dumped
                                # upon failure
                                redirect_stderr=subprocess.PIPE,
                                redirect_stdout=subprocess.PIPE)
  # Get the values returned from the llc RPC to use in input to ld
  is_shared = re.search(r'output\s+0:\s+i\(([0|1])\)', stdout).group(1)
  is_shared = (is_shared == '1')
  soname = re.search(r'output\s+1:\s+s\("(.*)"\)', stdout).group(1)
  needed_str = re.search(r'output\s+2:\s+s\("(.*)"\)', stdout).group(1)
  # If the delimiter changes, this line needs to change
  needed_libs = [ lib for lib in needed_str.split(r'\n') if lib]
  return is_shared, soname, needed_libs

def BuildLLCCommandLine(flags):
  # command_line is a NUL (\x00) terminated sequence.
  kTerminator = '\0'
  command_line = kTerminator.join(['llc'] + flags) + kTerminator
  command_line_escaped = command_line.replace(kTerminator, '\\x00')
  return len(command_line), command_line_escaped

def MakeSelUniversalScriptForLLC(infile, outfile, flags):
  script = []
  script.append('readwrite_file objfile %s' % outfile)
  stream_rate = int(env.getraw('BITCODE_STREAM_RATE'))
  assert stream_rate != 0
  if UseDefaultCommandlineLLC():
    script.append('rpc StreamInit h(objfile) * s()')
  else:
    cmdline_len, cmdline_escaped = BuildLLCCommandLine(flags)
    script.append('rpc StreamInitWithCommandLine h(objfile) C(%d,%s) * s()' %
                  (cmdline_len, cmdline_escaped))
  # specify filename, chunk size and rate in bits/s
  script.append('stream_file %s %s %s' % (infile, 64 * 1024, stream_rate))
  script.append('rpc StreamEnd * i() s() s() s()')
  script.append('echo "llc complete"')
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

  The output file type depends on the input file type:
     Portable object (.po)         -> NaCl ELF object (.o)
     Portable shared object (.pso) -> NaCl ELF shared object (.so)
     Portable executable (.pexe)   -> NaCl ELF executable (.nexe)

ADVANCED OPTIONS:
  -mattr=<+feat1,-feat2>  Toggle specific cpu features on and off.
  -mcpu=<cpu-name>        Target a specific cpu type.  Tunes code as well as
                          turns cpu features on and off.
  -S                      Generate native assembly only.
  -c                      Generate native object file only.
  --pnacl-sb              Use the translator which runs inside the NaCl sandbox.
  -O[0-3]                 Change translation-time optimization level.
"""
