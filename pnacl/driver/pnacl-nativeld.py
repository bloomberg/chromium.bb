#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
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
# The bulk of this file is logic to invoke the sandboxed translator
# (SRPC and non-SRPC).

from driver_tools import *
from driver_env import env
from driver_log import Log

EXTRA_ENV = {
  'INPUTS'   : '',
  'OUTPUT'   : '',
  'SHM_FILE' : '',

  'STDLIB': '1',
  'SHARED': '0',
  'STATIC': '0',
  'RELOCATABLE': '0',

  'LD_EMUL'        : '${LD_EMUL_%ARCH%}',
  'LD_EMUL_ARM'    : 'armelf_nacl',
  'LD_EMUL_X8632'  : 'elf_nacl',
  'LD_EMUL_X8664'  : 'elf64_nacl',

   # Intermediate variable LDMODE is used for delaying evaluation.
   # LD_SB is the non-srpc sandboxed tool.
  'LDMODE'      : '${SANDBOXED ? SB : BFD}',
  'LD'          : '${LD_%LDMODE%}',
  # --eh-frame-hdr asks the linker to generate an .eh_frame_hdr section,
  # which is a presorted list of registered frames. This section is
  # used by libgcc_eh/libgcc_s to avoid doing the sort during runtime.
  # http://www.airs.com/blog/archives/462
  'LD_FLAGS'    : '-nostdlib -m ${LD_EMUL} --eh-frame-hdr ' +
                  '${#LD_SCRIPT ? -T ${LD_SCRIPT}} ' +
                  '${STATIC ? -static} ${SHARED ? -shared} ${RELOCATABLE ? -r}',

  'EMITMODE'         : '${RELOCATABLE ? relocatable : ' +
                       '${STATIC ? static : ' +
                       '${SHARED ? shared : dynamic}}}',

  'LD_SCRIPT': '${STDLIB ? ' +
               '${LD_SCRIPT_%LIBMODE%_%EMITMODE%} : ' +
               '${LD_SCRIPT_nostdlib}}',

  'LD_SCRIPT_nostdlib' : '',

  # For newlib, omit the -T flag (the builtin linker script works fine).
  'LD_SCRIPT_newlib_static': '',

  # For glibc, the linker script is always explicitly specified.
  'LD_SCRIPT_glibc_static' : '${LD_EMUL}.x.static',
  'LD_SCRIPT_glibc_shared' : '${LD_EMUL}.xs',
  'LD_SCRIPT_glibc_dynamic': '${LD_EMUL}.x',

  'LD_SCRIPT_newlib_relocatable': '',
  'LD_SCRIPT_glibc_relocatable' : '',

  'LD_EMUL'        : '${LD_EMUL_%ARCH%}',
  'LD_EMUL_ARM'    : 'armelf_nacl',
  'LD_EMUL_X8632'  : 'elf_nacl',
  'LD_EMUL_X8664'  : 'elf64_nacl',

  'SEARCH_DIRS'        : '${SEARCH_DIRS_USER} ${SEARCH_DIRS_BUILTIN}',
  'SEARCH_DIRS_USER'   : '',
  'SEARCH_DIRS_BUILTIN': '${STDLIB ? ${LIBS_ARCH}/}',

  'LIBS_ARCH'        : '${LIBS_%ARCH%}',
  'LIBS_ARM'         : '${BASE_LIB_NATIVE}arm',
  'LIBS_X8632'       : '${BASE_LIB_NATIVE}x86-32',
  'LIBS_X8664'       : '${BASE_LIB_NATIVE}x86-64',

  'RUN_LD' : '${LD} ${LD_FLAGS} ${inputs} -o "${output}"',
}
env.update(EXTRA_ENV)

def PassThrough(*args):
  env.append('LD_FLAGS', *args)

LDPatterns = [
  ( '-o(.+)',          "env.set('OUTPUT', pathtools.normalize($0))"),
  ( ('-o', '(.+)'),    "env.set('OUTPUT', pathtools.normalize($0))"),

  ( ('(--add-extra-dt-needed=.*)'), PassThrough),
  ( '-shared',         "env.set('SHARED', '1')"),
  ( '-static',         "env.set('STATIC', '1')"),
  ( '-nostdlib',       "env.set('STDLIB', '0')"),
  ( '-r',              "env.set('RELOCATABLE', '1')"),
  ( '-relocatable',    "env.set('RELOCATABLE', '1')"),

  ( '-L(.+)',          "env.append('SEARCH_DIRS_USER', $0)"),
  ( ('-L', '(.*)'),    "env.append('SEARCH_DIRS_USER', $0)"),

  ( ('(-Ttext)','(.*)'), PassThrough),
  ( ('(-Ttext=.*)'),     PassThrough),

  # This overrides the builtin linker script.
  ( ('-T', '(.*)'),      "env.set('LD_SCRIPT', $0)"),

  ( ('(-e)','(.*)'),              PassThrough),
  ( '(-entry=.*)',                PassThrough),
  ( ('(--section-start)','(.*)'), PassThrough),
  ( '(-?-soname=.*)',             PassThrough),
  ( ('(-?-soname)', '(.*)'),      PassThrough),
  ( '(-M)',                       PassThrough),
  ( '(-t)',                       PassThrough),
  ( ('-y','(.*)'),                PassThrough),
  ( ('(-defsym)','(.*)'),         PassThrough),

  ( '(--print-gc-sections)',      PassThrough),
  ( '(-gc-sections)',             PassThrough),
  ( '(--unresolved-symbols=.*)',  PassThrough),

  ( '-melf_nacl',          "env.set('ARCH', 'X8632')"),
  ( ('-m','elf_nacl'),     "env.set('ARCH', 'X8632')"),
  ( '-melf64_nacl',        "env.set('ARCH', 'X8664')"),
  ( ('-m','elf64_nacl'),   "env.set('ARCH', 'X8664')"),
  ( '-marmelf_nacl',       "env.set('ARCH', 'ARM')"),
  ( ('-m','armelf_nacl'),  "env.set('ARCH', 'ARM')"),

  # Inputs and options that need to be kept in order
  ( '(--no-as-needed)',    "env.append('INPUTS', $0)"),
  ( '(--as-needed)',       "env.append('INPUTS', $0)"),
  ( '(--start-group)',     "env.append('INPUTS', $0)"),
  ( '(--end-group)',       "env.append('INPUTS', $0)"),
  ( '(-Bstatic)',          "env.append('INPUTS', $0)"),
  ( '(-Bdynamic)',         "env.append('INPUTS', $0)"),
  # This is the file passed using shmem
  ( ('--shm=(.*)'),        "env.append('INPUTS', $0)\n"
                           "env.set('SHM_FILE', $0)"),
  ( '(--(no-)?whole-archive)', "env.append('INPUTS', $0)"),

  ( '(-l.*)',              "env.append('INPUTS', $0)"),

  ( '(-.*)',               UnrecognizedOption),
  ( '(.*)',                "env.append('INPUTS', pathtools.normalize($0))"),
]

def main(argv):
  ParseArgs(argv, LDPatterns)

  GetArch(required=True)
  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')

  if output == '':
    output = pathtools.normalize('a.out')

  # Default to -static for newlib
  if env.getbool('LIBMODE_NEWLIB'):
    env.set('STATIC', '1')

  # Expand all parameters
  # This resolves -lfoo into actual filenames,
  # and expands linker scripts into command-line arguments.
  inputs = ldtools.ExpandInputs(inputs,
                                env.get('SEARCH_DIRS'),
                                env.getbool('STATIC'))

  # If there's a linker script which needs to be searched for, find it.
  LocateLinkerScript()

  env.push()
  env.set('inputs', *inputs)
  env.set('output', output)
  if env.getbool('SANDBOXED') and env.getbool('SRPC'):
    RunLDSRPC()
  else:
    RunWithLog('${RUN_LD}')
  env.pop()
  return 0

def IsFlag(arg):
  return arg.startswith('-')

def RunLDSRPC():
  CheckTranslatorPrerequisites()
  # The "main" input file is the application's combined object file.
  all_inputs = env.get('inputs')

  main_input = env.getone('SHM_FILE')
  if not main_input:
    Log.Fatal("Sandboxed LD requires one shm input file")

  outfile = env.getone('output')

  files = LinkerFiles(all_inputs)
  ld_flags = env.get('LD_FLAGS')

  # In this mode, we assume we only use the builtin linker script.
  assert(env.getone('LD_SCRIPT') == '')
  script = MakeSelUniversalScriptForLD(ld_flags,
                                       main_input,
                                       files,
                                       outfile)

  RunWithLog('${SEL_UNIVERSAL_PREFIX} ${SEL_UNIVERSAL} ' +
             '${SEL_UNIVERSAL_FLAGS} -- ${LD_SRPC}',
             stdin=script, echo_stdout = False, echo_stderr = False)

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
  # Create a mapping for each input file and add it to the command line.
  for f in files:
    basename = pathtools.basename(f)
    script.append('reverse_service_add_manifest_mapping files/%s %s' %
                  (basename, f))

  use_default = env.getbool('USE_DEFAULT_CMD_LINE')
  if use_default:
    basename = pathtools.basename(main_input)
    script.append('readonly_file objfile %s' % main_input)
    script.append('rpc RunWithDefaultCommandLine '
                  'h(objfile) h(nexefile) i(0) s("") s("") *')
  else:
    # Join all the arguments.
    # Based on the format of RUN_LD, the order of arguments is:
    # ld_flags, then input files (which are more sensitive to order).
    kTerminator = '\0'
    command_line = kTerminator.join(ld_flags) + kTerminator
    for f in files:
      basename = pathtools.basename(f)
      command_line = command_line + basename + kTerminator
    command_line_escaped = command_line.replace(kTerminator, '\\x00')
    script.append('rpc Run h(nexefile) i(0) s("") s("") C(%d,%s) *' %
                  (len(command_line), command_line_escaped))
  script.append('echo "ld complete"')
  script.append('')
  return '\n'.join(script)

# Given linker arguments (including -L, -l, and filenames),
# returns the list of files which are pulled by the linker.
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

def LocateLinkerScript():
  ld_script = env.getone('LD_SCRIPT')
  if not ld_script:
    # No linker script specified
    return

  # See if it's an absolute or relative path
  path = pathtools.normalize(ld_script)
  if pathtools.exists(path):
    env.set('LD_SCRIPT', path)
    return

  # Search for the script
  search_dirs = [pathtools.normalize('.')] + env.get('SEARCH_DIRS')
  path = ldtools.FindFile([ld_script], search_dirs)
  if not path:
    Log.Fatal("Unable to find linker script '%s'", ld_script)

  env.set('LD_SCRIPT', path)

if __name__ == "__main__":
  DriverMain(main)
