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

  # Capture some metadata passed from pnacl-translate over to here.
  'SONAME' : '',

  'STDLIB': '1',
  'SHARED': '0',
  'STATIC': '0',
  'RELOCATABLE': '0',

  # These are used only for the static cases
  # For the shared/dynamic case it does not really make sense
  # to change them as there are assumptions made all over the
  # place about them.
  'BASE_TEXT': '0x20000',
  'BASE_RODATA': '0x10020000',

  # Determine if we should build nexes compatible with the IRT.
  'USE_IRT' : '1',

  # We consider 4 different gold modes.
  # NOTE: "shared" implies PIC
  #       "dynamic" should probbably imply nonPIC to avoid
  #                 tls related instruction rewrites by the linker
  # NOTE: gold does not use a linker script so you have to disable
  #       them besides setting 'USE_GOLD' to '0'.
  # NOTE: -Tdata is used to influence the placement of the ro segment
  #       gold has been adjusted accordingly
  'LD_FLAGS_static': '--rosegment --native-client ' +
                     '--keep-headers-out-of-load-segment ' +
                     '${USE_IRT ? -Tdata=${BASE_RODATA}} -Ttext=${BASE_TEXT}',

  'LD_FLAGS_shared': '--rosegment --bsssegment --native-client ' +
                     '--keep-headers-out-of-load-segment ' +
                     '-Tdata=0x10000000',

  'LD_FLAGS_dynamic': '--rosegment  --bsssegment --native-client ' +
                      '--keep-headers-out-of-load-segment ' +
                      '-Tdata=0x11000000 -Ttext=0x1000000',

  # --eh-frame-hdr asks the linker to generate an .eh_frame_hdr section,
  # which is a presorted list of registered frames. This section is
  # used by libgcc_eh/libgcc_s to avoid doing the sort during runtime.
  # http://www.airs.com/blog/archives/462
  #
  'LD_FLAGS'    : '${LD_FLAGS_%EMITMODE%} ' +
                  '-nostdlib ' +
                  # Only relvevant for ARM where it suppresses a warning.
                  # Ignored for other archs.
                  '--no-fix-cortex-a8 ' +
                  '-m ${LD_EMUL} ' +
                  '--eh-frame-hdr ' +
                  '${#SONAME ? -soname=${SONAME}} ' +
                  '${STATIC ? -static} ${SHARED ? -shared} ${RELOCATABLE ? -r}',

  # This may contain the metadata file, which is passed to LD with --metadata.
  # It must be passed at the end of the link line.
  'METADATA_FILE': '',
  'NEEDED_LIBRARIES': '',

  'EMITMODE'         : '${RELOCATABLE ? relocatable : ' +
                       '${STATIC ? static : ' +
                       '${SHARED ? shared : dynamic}}}',

  'LD_EMUL'        : '${LD_EMUL_%ARCH%}',
  'LD_EMUL_ARM'    : 'armelf_nacl',
  'LD_EMUL_X8632'  : 'elf_nacl',
  'LD_EMUL_X8664'  : 'elf64_nacl',
  'LD_EMUL_MIPS32' : 'elf32ltsmip_nacl',

  'SEARCH_DIRS'        : '${SEARCH_DIRS_USER} ${SEARCH_DIRS_BUILTIN}',
  'SEARCH_DIRS_USER'   : '',
  'SEARCH_DIRS_BUILTIN': '${STDLIB ? ${LIBS_ARCH}/}',

  'LIBS_ARCH'        : '${LIBS_%ARCH%}',
  'LIBS_ARM'         : '${BASE_LIB_NATIVE}arm',
  'LIBS_X8632'       : '${BASE_LIB_NATIVE}x86-32',
  'LIBS_X8664'       : '${BASE_LIB_NATIVE}x86-64',
  'LIBS_MIPS32'      : '${BASE_LIB_NATIVE}mips32',

  # Note: this is only used in the unsandboxed case
  'RUN_LD' : '${LD} ${LD_FLAGS} ${inputs} -o ${output}' +
             '${#METADATA_FILE ? --metadata ${METADATA_FILE}}',
}

def PassThrough(*args):
  env.append('LD_FLAGS', *args)

LDPatterns = [
  ( '-o(.+)',          "env.set('OUTPUT', pathtools.normalize($0))"),
  ( ('-o', '(.+)'),    "env.set('OUTPUT', pathtools.normalize($0))"),

  ( ('(--add-extra-dt-needed=.*)'), "env.append('NEEDED_LIBRARIES', $0)"),
  ( ('--metadata', '(.+)'),         "env.set('METADATA_FILE', $0)"),
  ( '--noirt',                      "env.set('USE_IRT', '0')"),

  ( '-shared',         "env.set('SHARED', '1')"),
  ( '-static',         "env.set('STATIC', '1')"),
  ( '-nostdlib',       "env.set('STDLIB', '0')"),
  ( '-r',              "env.set('RELOCATABLE', '1')"),
  ( '-relocatable',    "env.set('RELOCATABLE', '1')"),

  ( '-L(.+)',
    "env.append('SEARCH_DIRS_USER', pathtools.normalize($0))"),
  ( ('-L', '(.*)'),
    "env.append('SEARCH_DIRS_USER', pathtools.normalize($0))"),
  # Note: we conflate '-Ttext' and '--section-start .text=' here
  # This is not quite right but it is the intention of the tests
  # using the flags which want to control the placement of the
  # "rx" and "r" segments
  ( ('-Ttext','(.*)'),                  "env.set('BASE_TEXT', $0)"),
  ( ('-Ttext=(.*)'),                    "env.set('BASE_TEXT', $0)"),
  ( ('--section-start','.text=(.*)'),   "env.set('BASE_TEXT', $0)"),
  ( ('--section-start','.rodata=(.*)'), "env.set('BASE_RODATA', $0)"),

  ( ('(-e)','(.*)'),              PassThrough),
  ( '(--entry=.*)',               PassThrough),
  ( '-?-soname=(.*)',             "env.set('SONAME', $0)"),
  ( ('(-?-soname)', '(.*)'),      "env.set('SONAME', $1)"),
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

  if GetArch(required=True) == 'X8664':
    env.append('LD_FLAGS', '--metadata-is64')

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
    # We don't have the bitcode file here anymore, so we assume that
    # pnacl-translate passed along the relevant information via commandline.
    soname = env.getone('SONAME')
    needed = env.get('NEEDED_LIBRARIES')
    emit_mode = env.getone('EMITMODE')
    if emit_mode == 'shared':
      is_shared_library = 1
    else:
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

    # Add the metadata file
    metadata_file = env.getone('METADATA_FILE')
    if metadata_file:
      command_line = command_line + '--metadata' + kTerminator
      command_line = command_line + metadata_file + kTerminator

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
