#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#
# This is a thin wrapper for native LD. This is not meant to be
# used by the user, but is called from pnacl-translate.
# This implements the native linking part of translation.
#
# All inputs must be native objects or linker scripts.
#
# --pnacl-sb will cause the sandboxed LD to be used.
# The bulk of this file is logic to invoke the sandboxed translator.

from driver_tools import *
from driver_env import env
from driver_log import Log

import subprocess

EXTRA_ENV = {
  'INPUTS'   : '',
  'OUTPUT'   : '',

  # the INPUTS file coming from the llc translation step
  'LLC_TRANSLATED_FILE' : '',
  'USE_STDLIB': '1',

  # Determine if we should build nexes compatible with the IRT.
  'USE_IRT' : '1',

  # Upstream gold has the segment gap built in, but the gap can be modified
  # when not using the IRT. The gap does need to be at least one bundle so the
  # halt sled can be added for the TCB in case the segment ends up being a
  # multiple of 64k.
  # --eh-frame-hdr asks the linker to generate an .eh_frame_hdr section,
  # which is a presorted list of registered frames. This section is
  # used by libgcc_eh/libgcc_s to avoid doing the sort during runtime.
  # http://www.airs.com/blog/archives/462
  #
  'LD_FLAGS'    : '-nostdlib ' +
                  # Only relevant for ARM where it suppresses a warning.
                  # Ignored for other archs.
                  '--no-fix-cortex-a8 ' +
                  '-m ${LD_EMUL} ' +
                  '--eh-frame-hdr ' +
                  '-static ' +
                  '--build-id ' +
                  '${!USE_IRT ? --rosegment-gap=32}',

  'LD_EMUL'        : '${LD_EMUL_%ARCH%}',
  'LD_EMUL_ARM'    : 'armelf_nacl',
  'LD_EMUL_X8632'  : 'elf_nacl',
  'LD_EMUL_X8664'  : 'elf64_nacl',
  'LD_EMUL_MIPS32' : 'elf32ltsmip_nacl',

  'SEARCH_DIRS'        : '${SEARCH_DIRS_USER} ${SEARCH_DIRS_BUILTIN}',
  'SEARCH_DIRS_USER'   : '',
  'SEARCH_DIRS_BUILTIN': '${USE_STDLIB ? ${LIBS_ARCH}/}',

  'LIBS_ARCH'        : '${LIBS_%ARCH%}',
  'LIBS_ARM'         : '${BASE_LIB_NATIVE}arm',
  'LIBS_X8632'       : '${BASE_LIB_NATIVE}x86-32',
  'LIBS_X8664'       : '${BASE_LIB_NATIVE}x86-64',
  'LIBS_MIPS32'      : '${BASE_LIB_NATIVE}mips32',

  # Note: this is only used in the unsandboxed case
  'RUN_LD' : '${LD} ${LD_FLAGS} ${inputs} -o ${output}'
}

def PassThrough(*args):
  env.append('LD_FLAGS', *args)

LDPatterns = [
  ( '-o(.+)',          "env.set('OUTPUT', pathtools.normalize($0))"),
  ( ('-o', '(.+)'),    "env.set('OUTPUT', pathtools.normalize($0))"),

  ( '--noirt',                      "env.set('USE_IRT', '0')"),

  ( '-static',         "env.set('STATIC', '1')"),
  ( '-nostdlib',       "env.set('USE_STDLIB', '0')"),

  ( '-L(.+)',
    "env.append('SEARCH_DIRS_USER', pathtools.normalize($0))"),
  ( ('-L', '(.*)'),
    "env.append('SEARCH_DIRS_USER', pathtools.normalize($0))"),

  # Note: we do not yet support all the combinations of flags which affect
  # layout of the various sections and segments because the corner cases in gold
  # may not all be worked out yet. They can be added (and tested!) as needed.
  ( ('(-Ttext=.*)'),              PassThrough),
  ( ('(-Trodata=.*)'),            PassThrough),
  ( ('(-Ttext-segment=.*)'),      PassThrough),
  ( ('(-Trodata-segment=.*)'),    PassThrough),
  ( ('(--section-start)', '(.+)'),PassThrough),
  ( ('(--section-start=.*)'),     PassThrough),
  ( ('(-e)','(.*)'),              PassThrough),
  ( '(--entry=.*)',               PassThrough),
  ( '(-M)',                       PassThrough),
  ( '(-t)',                       PassThrough),
  ( ('-y','(.*)'),                PassThrough),
  ( ('(-defsym)','(.*)'),         PassThrough),
  ( '-export-dynamic',            PassThrough),

  ( '(--print-gc-sections)',      PassThrough),
  ( '(--gc-sections)',            PassThrough),
  ( '(--unresolved-symbols=.*)',  PassThrough),
  ( '(--dynamic-linker=.*)',      PassThrough),
  ( '(-g)',                       PassThrough),
  ( '(--build-id)',               PassThrough),

  ( '-melf_nacl',            "env.set('ARCH', 'X8632')"),
  ( ('-m','elf_nacl'),       "env.set('ARCH', 'X8632')"),
  ( '-melf64_nacl',          "env.set('ARCH', 'X8664')"),
  ( ('-m','elf64_nacl'),     "env.set('ARCH', 'X8664')"),
  ( '-marmelf_nacl',         "env.set('ARCH', 'ARM')"),
  ( ('-m','armelf_nacl'),    "env.set('ARCH', 'ARM')"),
  ( '-mmipselelf_nacl',      "env.set('ARCH', 'MIPS32')"),
  ( ('-m','mipselelf_nacl'), "env.set('ARCH', 'MIPS32')"),

  # Inputs and options that need to be kept in order
  ( '(--no-as-needed)',    "env.append('INPUTS', $0)"),
  ( '(--as-needed)',       "env.append('INPUTS', $0)"),
  ( '(--start-group)',     "env.append('INPUTS', $0)"),
  ( '(--end-group)',       "env.append('INPUTS', $0)"),
  ( '(-Bstatic)',          "env.append('INPUTS', $0)"),
  ( '(-Bdynamic)',         "env.append('INPUTS', $0)"),
  # This is the file passed from llc during translation (used to be via shmem)
  ( ('--llc-translated-file=(.*)'), "env.append('INPUTS', $0)\n"
                                    "env.set('LLC_TRANSLATED_FILE', $0)"),
  ( '(--(no-)?whole-archive)', "env.append('INPUTS', $0)"),

  ( '(-l.*)',              "env.append('INPUTS', $0)"),
  ( '(--undefined=.*)',    "env.append('INPUTS', $0)"),

  ( '(-.*)',               UnrecognizedOption),
  ( '(.*)',                "env.append('INPUTS', pathtools.normalize($0))"),
]


def main(argv):
  env.update(EXTRA_ENV)

  ParseArgs(argv, LDPatterns)

  GetArch(required=True)
  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')

  if output == '':
    output = pathtools.normalize('a.out')

  # Expand all parameters
  # This resolves -lfoo into actual filenames,
  # and expands linker scripts into command-line arguments.
  inputs = ldtools.ExpandInputs(inputs,
                                env.get('SEARCH_DIRS'),
                                env.getbool('STATIC'),
                                ldtools.LibraryTypes.NATIVE)

  env.push()
  env.set('inputs', *inputs)
  env.set('output', output)

  if env.getbool('SANDBOXED'):
    RunLDSandboxed()
  else:
    Run('${RUN_LD}')
  env.pop()
  # only reached in case of no errors
  return 0

def IsFlag(arg):
  return arg.startswith('-')

def RunLDSandboxed():
  CheckTranslatorPrerequisites()
  # The "main" input file is the application's combined object file.
  all_inputs = env.get('inputs')

  main_input = env.getone('LLC_TRANSLATED_FILE')
  if not main_input:
    Log.Fatal("Sandboxed LD requires one shm input file")

  outfile = env.getone('output')

  files = LinkerFiles(all_inputs)
  ld_flags = env.get('LD_FLAGS')

  script = MakeSelUniversalScriptForLD(ld_flags,
                                       main_input,
                                       files,
                                       outfile)


  Run('${SEL_UNIVERSAL_PREFIX} ${SEL_UNIVERSAL} ' +
      '${SEL_UNIVERSAL_FLAGS} -- ${LD_SB}',
      stdin_contents=script,
      # stdout/stderr will be automatically dumped
      # upon failure
      redirect_stderr=subprocess.PIPE,
      redirect_stdout=subprocess.PIPE)


def MakeSelUniversalScriptForLD(ld_flags,
                                main_input,
                                files,
                                outfile):
  """ Return sel_universal script text for invoking LD.nexe with the
      given ld_flags, main_input (which is treated specially), and
      other input files. The output will be written to outfile.  """
  script = []

  # Open the output file.
  script.append('readwrite_file nexefile %s' % outfile)

  # Need to include the linker script file as a linker resource for glibc.
  files_to_map = list(files)

  use_default = env.getbool('USE_DEFAULT_CMD_LINE')
  # Create a mapping for each input file and add it to the command line.
  for f in files_to_map:
    basename = pathtools.basename(f)
    # If we are using the dummy shim, map it with the filename of the real
    # shim, so the baked-in commandline will work
    if basename == 'libpnacl_irt_shim_dummy.a' and use_default:
       basename = 'libpnacl_irt_shim.a'
    script.append('reverse_service_add_manifest_mapping files/%s %s' %
                  (basename, f))

  if use_default:
    # Assume static linking for now.
    soname = ''
    needed = ''
    is_shared_library = 0

    script.append('readonly_file objfile %s' % main_input)
    script.append(('rpc RunWithDefaultCommandLine ' +
                   'h(objfile) h(nexefile) i(%d) s("%s") s("%s") *'
                   % (is_shared_library, soname, needed)))
  else:
    # Join all the arguments.
    # Based on the format of RUN_LD, the order of arguments is:
    # ld_flags, then input files (which are more sensitive to order).

    # For the sandboxed build, we don't want "real" filesystem paths,
    # because we use the reverse-service to do a lookup -- make sure
    # everything is a basename first.
    ld_flags = [ pathtools.basename(flag) for flag in ld_flags ]
    kTerminator = '\0'
    command_line = kTerminator.join(['ld'] + ld_flags) + kTerminator
    for f in files:
      basename = pathtools.basename(f)
      command_line = command_line + basename + kTerminator

    command_line_escaped = command_line.replace(kTerminator, '\\x00')
    # Assume that the commandline captures all necessary metadata for now.
    script.append('rpc Run h(nexefile) C(%d,%s) *' %
                  (len(command_line), command_line_escaped))
  script.append('echo "ld complete"')
  script.append('')
  return '\n'.join(script)

# Given linker arguments (including -L, -l, and filenames),
# returns the list of files which are pulled by the linker,
# with real path names set set up the real -> flat name mapping.
def LinkerFiles(args):
  ret = []
  for f in args:
    if IsFlag(f):
      continue
    else:
      if not pathtools.exists(f):
        Log.Fatal("Unable to open '%s'", pathtools.touser(f))
      ret.append(f)
  return ret
