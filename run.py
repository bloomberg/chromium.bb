#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import platform
import subprocess
import sys
import tempfile

class Environment:
  pass
env = Environment()

def SetupEnvironment():
  # linux, win, or mac
  env.scons_os = GetSconsOS()

  # native_client/ directory
  env.nacl_root = FindBaseDir()

  # Path to Native NaCl toolchain (glibc)
  env.nnacl_root = os.path.join(env.nacl_root,
                                'toolchain',
                                env.scons_os + '_x86')

  # Path to PNaCl toolchain
  pnacl_label = 'pnacl_%s_x86' % GetSconsOS()
  env.pnacl_base = os.path.join(env.nacl_root, 'toolchain', pnacl_label)

  # QEMU
  env.arm_root = os.path.join(env.nacl_root,
                              'toolchain', 'linux_arm-trusted')
  env.qemu_arm = os.path.join(env.arm_root, 'run_under_qemu_arm')

  env.mips32_root = os.path.join(env.nacl_root,
                                 'toolchain', 'linux_mips-trusted')
  env.qemu_mips32 = os.path.join(env.mips32_root, 'run_under_qemu_mips32')

  # Path to 'readelf'
  env.readelf = FindReadElf()

  # Path to 'scons'
  env.scons = os.path.join(env.nacl_root, 'scons')

  # Library path for runnable-ld.so
  env.library_path = []

  # Suppress -S -a
  env.paranoid = False

  # Only print commands, don't run them
  env.dry_run = False

  # Force a specific sel_ldr
  env.force_sel_ldr = None

  # Force a specific IRT
  env.force_irt = None

  # Don't print anything
  env.quiet = False

  # Arch (x86-32, x86-64, arm, mips32)
  env.arch = None

  # Trace in QEMU
  env.trace = False

  # Debug the nexe using the debug stub
  env.debug = False

def PrintBanner(output):
  if not env.quiet:
    lines = output.split('\n')
    print '*' * 80
    for line in lines:
      padding = ' ' * max(0, (80 - len(line)) / 2)
      print padding + output + padding
    print '*' * 80

def PrintCommand(s):
  if not env.quiet:
    print
    print s
    print

def GetMultiDir(arch):
  if arch == 'x86-32':
    return 'lib32'
  elif arch == 'x86-64':
    return 'lib'
  else:
    Fatal('nacl-gcc does not support %s' % arch)

def SetupArch(arch, allow_build=True):
  '''Setup environment variables that require knowing the
     architecture. We can only do this after we've seen the
     nexe or once we've read -arch off the command-line.
  '''
  env.arch = arch
  env.sel_ldr = FindOrBuildSelLdr(allow_build=allow_build)
  env.irt = FindOrBuildIRT(allow_build=allow_build)


def SetupLibC(arch, is_pnacl, is_dynamic):
  if is_dynamic:
    if is_pnacl:
      libdir = os.path.join(env.pnacl_base, 'lib-' + arch)
    else:
      libdir = os.path.join(env.nnacl_root, 'x86_64-nacl', GetMultiDir(arch))
    env.runnable_ld = os.path.join(libdir, 'runnable-ld.so')
    env.library_path.append(libdir)


def main(argv):
  SetupEnvironment()
  return_code = 0

  sel_ldr_options = []
  # sel_ldr's "quiet" options need to come early in the command line
  # to suppress noisy output from processing other options, like -Q.
  sel_ldr_quiet_options = []
  nexe, nexe_params = ArgSplit(argv[1:])

  # Translate .pexe files
  is_pnacl = nexe.endswith('.pexe')

  try:
    if is_pnacl:
      nexe = Translate(env.arch, nexe)

    # Read the ELF file info
    if is_pnacl and env.dry_run:
      # In a dry run, we don't actually run pnacl-translate, so there is
      # no nexe for readelf. Fill in the information manually.
      arch = env.arch
      is_dynamic = False
      is_glibc_static = False
    else:
      arch, is_dynamic, is_glibc_static = ReadELFInfo(nexe)

    # Add default sel_ldr options
    if not env.paranoid:
      sel_ldr_options += ['-S', '-a']
      # X86-64 glibc static has validation problems without stub out (-s)
      if arch == 'x86-64' and is_glibc_static:
        sel_ldr_options += ['-s']
    if env.quiet:
      # Don't print sel_ldr logs
      # These need to be at the start of the arglist for full effectiveness.
      # -q means quiet most stderr warnings.
      # -l /dev/null means log to /dev/null.
      sel_ldr_quiet_options = ['-q', '-l', '/dev/null']
    if env.debug:
      # Disabling validation (-c) is used by the debug stub test.
      # TODO(dschuff): remove if/when it's no longer necessary
      sel_ldr_options += ['-c', '-c', '-g']

    # Tell the user
    if is_dynamic:
      extra = 'DYNAMIC'
    else:
      extra = 'STATIC'
    PrintBanner('%s is %s %s' % (os.path.basename(nexe),
                                 arch.upper(), extra))

    # Setup architecture-specific environment variables
    SetupArch(arch)

    # Setup LibC-specific environment variables
    SetupLibC(arch, is_pnacl, is_dynamic)

    sel_ldr_args = []

    # Add irt to sel_ldr arguments
    if env.irt:
      sel_ldr_args += ['-B', env.irt]

    # Setup sel_ldr arguments
    sel_ldr_args += sel_ldr_options + ['--']

    if is_dynamic:
      sel_ldr_args += [env.runnable_ld,
                       '--library-path', ':'.join(env.library_path)]

    sel_ldr_args += [os.path.abspath(nexe)] + nexe_params

    # Run sel_ldr!
    retries = 0
    try:
      if hasattr(env, 'retries'):
        retries = int(env.retries)
    except ValueError:
      pass
    collate = env.collate or retries > 0
    input = sys.stdin.read() if collate else None
    for iter in range(1 + max(retries, 0)):
      output = RunSelLdr(sel_ldr_args, quiet_args=sel_ldr_quiet_options,
                         collate=collate, stdin_string=input)
      if env.last_return_code < 128:
        # If the application crashes, we expect a 128+ return code.
        break
    sys.stdout.write(output or '')
    return_code = env.last_return_code
  finally:
    if is_pnacl:
      # Clean up the .nexe that was created.
      try:
        os.remove(nexe)
      except:
        pass

  return return_code


def RunSelLdr(args, quiet_args=[], collate=False, stdin_string=None):
  """Run the sel_ldr command and optionally capture its output.

  Args:
    args: A string list containing the command and arguments.
    collate: Whether to capture stdout+stderr (rather than passing
        them through to the terminal).
    stdin_string: Text to send to the command via stdin.  If None, stdin is
        inherited from the caller.

  Returns:
    A string containing the concatenation of any captured stdout plus
    any captured stderr.
  """
  prefix = []
  # The bootstrap loader args (--r_debug, --reserved_at_zero) need to
  # come before quiet_args.
  bootstrap_loader_args = []
  if GetBuildArch().find('arm') == -1 and env.arch == 'arm':
    prefix = [ env.qemu_arm, '-cpu', 'cortex-a9']
    if env.trace:
      prefix += ['-d', 'in_asm,op,exec,cpu']
    args = ['-Q'] + args

  if GetBuildArch().find('mips32') == -1 and env.arch == 'mips32':
    prefix = [env.qemu_mips32]
    if env.trace:
      prefix += ['-d', 'in_asm,op,exec,cpu']
    args = ['-Q'] + args

  # Use the bootstrap loader on linux.
  if env.scons_os == 'linux':
    bootstrap = os.path.join(os.path.dirname(env.sel_ldr),
                             'nacl_helper_bootstrap')
    loader = [bootstrap, env.sel_ldr]
    template_digits = 'X' * 16
    bootstrap_loader_args = ['--r_debug=0x' + template_digits,
                             '--reserved_at_zero=0x' + template_digits]
  else:
    loader = [env.sel_ldr]
  return Run(prefix + loader + bootstrap_loader_args + quiet_args + args,
             exit_on_failure=(not collate),
             capture_stdout=collate, capture_stderr=collate,
             stdin_string=stdin_string)


def FindOrBuildIRT(allow_build = True):
  if env.force_irt:
    if env.force_irt == 'none':
      return None
    elif env.force_irt == 'core':
      flavors = ['irt_core']
    else:
      irt = env.force_irt
      if not os.path.exists(irt):
        Fatal('IRT not found: %s' % irt)
      return irt
  else:
    flavors = ['irt_core']

  irt_paths = []
  for flavor in flavors:
    path = os.path.join(env.nacl_root, 'scons-out',
                        'nacl_irt-%s/staging/%s.nexe' % (env.arch, flavor))
    irt_paths.append(path)

  for path in irt_paths:
    if os.path.exists(path):
      return path

  if allow_build:
    PrintBanner('irt not found. Building it with scons.')
    irt = irt_paths[0]
    BuildIRT(flavors[0])
    assert(env.dry_run or os.path.exists(irt))
    return irt

  return None

def BuildIRT(flavor):
  args = ('platform=%s naclsdk_validate=0 ' +
          'sysinfo=0 -j8 %s') % (env.arch, flavor)
  args = args.split()
  Run([env.scons] + args, cwd=env.nacl_root)

def FindOrBuildSelLdr(allow_build=True):
  if env.force_sel_ldr:
    if env.force_sel_ldr in ('dbg','opt'):
      modes = [ env.force_sel_ldr ]
    else:
      sel_ldr = env.force_sel_ldr
      if not os.path.exists(sel_ldr):
        Fatal('sel_ldr not found: %s' % sel_ldr)
      return sel_ldr
  else:
    modes = ['opt','dbg']

  loaders = []
  for mode in modes:
    sel_ldr = os.path.join(env.nacl_root, 'scons-out',
                           '%s-%s-%s' % (mode, env.scons_os, env.arch),
                           'staging', 'sel_ldr')
    loaders.append(sel_ldr)

  # If one exists, use it.
  for sel_ldr in loaders:
    if os.path.exists(sel_ldr):
      return sel_ldr

  # Build it
  if allow_build:
    PrintBanner('sel_ldr not found. Building it with scons.')
    sel_ldr = loaders[0]
    BuildSelLdr(modes[0])
    assert(env.dry_run or os.path.exists(sel_ldr))
    return sel_ldr

  return None

def BuildSelLdr(mode):
  args = ('platform=%s MODE=%s-host naclsdk_validate=0 ' +
          'sysinfo=0 -j8 sel_ldr') % (env.arch, mode)
  args = args.split()
  Run([env.scons] + args, cwd=env.nacl_root)

def Translate(arch, pexe):
  if arch is None:
    Fatal('Missing -arch for PNaCl translation.')
  output_file = os.path.splitext(pexe)[0] + '.' + arch + '.nexe'
  pnacl_translate = os.path.join(env.pnacl_base, 'bin', 'pnacl-translate')
  args = [ pnacl_translate, '-arch', arch, pexe, '-o', output_file,
           '--allow-llvm-bitcode-input' ]
  Run(args)
  return output_file

def Stringify(args):
  ret = ''
  for arg in args:
    if ' ' in arg:
      ret += ' "%s"' % arg
    else:
      ret += ' %s' % arg
  return ret.strip()

def PrepareStdin(stdin_string):
  """Prepare a stdin stream for a subprocess based on contents of a string.
  This has to be in the form of an actual file, rather than directly piping
  the string, since the child may (inappropriately) try to fseek() on stdin.

  Args:
    stdin_string: The characters to pipe to the subprocess.

  Returns:
    An open temporary file object ready to be read from.
  """
  f = tempfile.TemporaryFile()
  f.write(stdin_string)
  f.seek(0)
  return f

def Run(args, cwd=None, verbose=True, exit_on_failure=False,
        capture_stdout=False, capture_stderr=False, stdin_string=None):
  """Run a command and optionally capture its output.

  Args:
    args: A string list containing the command and arguments.
    cwd: Change to this directory before running.
    verbose: Print the command before running it.
    exit_on_failure: Exit immediately if the command returns nonzero.
    capture_stdout: Capture the stdout as a string (rather than passing it
        through to the terminal).
    capture_stderr: Capture the stderr as a string (rather than passing it
        through to the terminal).
    stdin_string: Text to send to the command via stdin.  If None, stdin is
        inherited from the caller.

  Returns:
    A string containing the concatenation of any captured stdout plus
    any captured stderr.
  """
  if verbose:
    PrintCommand(Stringify(args))
    if env.dry_run:
      return

  stdout_redir = None
  stderr_redir = None
  stdin_redir = None
  if capture_stdout:
    stdout_redir = subprocess.PIPE
  if capture_stderr:
    stderr_redir = subprocess.PIPE
  if stdin_string:
    stdin_redir = PrepareStdin(stdin_string)

  p = None
  try:
    p = subprocess.Popen(args, stdin=stdin_redir, stdout=stdout_redir,
                         stderr=stderr_redir, cwd=cwd)
    (stdout_contents, stderr_contents) = p.communicate()
  except KeyboardInterrupt, e:
    if p:
      p.kill()
    raise e
  except BaseException, e:
    if p:
      p.kill()
    raise e

  env.last_return_code = p.returncode

  if p.returncode != 0 and exit_on_failure:
    if capture_stdout or capture_stderr:
      # Print an extra message if any of the program's output wasn't
      # going to the screen.
      Fatal('Failed to run: %s' % Stringify(args))
    sys.exit(p.returncode)

  return (stdout_contents or '') + (stderr_contents or '')

def ArchDict():
  """Returns a dictionary that maps an alias for an architecture into
  its canonical architecture name.
  """
  alias = { 'x86-32': 'x86-32 x86_32 x8632 i386 i686 ia32 32',
            'x86-64': 'x86-64 amd64 x86_64 x8664 64',
            'arm'   : 'arm armv7',
            'mips32': 'mips32 mips' }
  return {alt:arch for arch in alias.keys() for alt in (alias[arch].split())}

def ArgSplit(argv):
  """Parse command-line arguments.

  Returns:
    Tuple (nexe, nexe_args) where nexe is the name of the nexe or pexe
    to execute, and nexe_args are its runtime arguments.
  """
  desc = ('Run a command-line nexe (or pexe). Automatically handles\n' +
          'translation, building sel_ldr, and building the IRT.')
  parser = argparse.ArgumentParser(description=desc)
  parser.add_argument('-L', action='append', dest='library_path', default=[],
                      help='Additional library path for runnable-ld.so.')
  parser.add_argument('--paranoid', action='store_true', default=False,
                      help='Remove -S (signals) and -a (file access) ' +
                      'from the default sel_ldr options.')
  parser.add_argument('--loader', dest='force_sel_ldr', metavar='SEL_LDR',
                      help='Path to sel_ldr.  "dbg" or "opt" means use ' +
                      'dbg or opt version of sel_ldr. ' +
                      'By default, use whichever sel_ldr already exists; ' +
                      'otherwise, build opt version.')
  parser.add_argument('--irt', dest='force_irt', metavar='IRT',
                      help='Path to IRT nexe.  "core" or "none" means use ' +
                      'Core IRT or no IRT.  By default, use whichever IRT ' +
                      'already exists; otherwise, build irt_core.')
  parser.add_argument('--dry-run', '-n', action='store_true', default=False,
                      help="Just print commands, don't execute them.")
  parser.add_argument('--quiet', '-q', action='store_true', default=False,
                      help="Don't print anything.")
  parser.add_argument('--retries', default='0', metavar='N',
                      help='Retry sel_ldr command up to N times (if ' +
                      'flakiness is expected).  This argument implies ' +
                      '--collate.')
  parser.add_argument('--collate', action='store_true', default=False,
                      help="Combine/collate sel_ldr's stdout and stderr, and " +
                      "print to stdout.")
  parser.add_argument('--trace', '-t', action='store_true', default=False,
                      help='Trace qemu execution.')
  parser.add_argument('--debug', '-g', action='store_true', default=False,
                      help='Run sel_ldr with debugging enabled.')
  parser.add_argument('-arch', '-m', dest='arch', action='store',
                      choices=sorted(ArchDict().keys()),
                      help='Specify architecture for PNaCl translation.')
  parser.add_argument('remainder', nargs=argparse.REMAINDER,
                      metavar='nexe/pexe + args')
  (options, args) = parser.parse_known_args(argv)
  # Copy the options into env.
  for (key, value) in vars(options).iteritems():
    setattr(env, key, value)
  # Canonicalize env.arch.  If it is None, it will be initialized later.
  aliases = ArchDict()
  if env.arch in aliases:
    env.arch = aliases[env.arch]
  elif env.arch:
    Fatal('Unrecognized arch "%s"!', env.arch)
  args += options.remainder
  nexe = args[0] if len(args) else ''
  return nexe, args[1:]

def Fatal(msg, *args):
  if len(args) > 0:
    msg = msg % args
  print msg
  sys.exit(1)


def FindReadElf():
  '''Returns the path of "readelf" binary.'''

  candidates = []
  # Use PNaCl's if it available.
  # TODO(robertm): standardize on one of the pnacl dirs
  candidates.append(
    os.path.join(env.pnacl_base, 'host', 'bin', 'arm-pc-nacl-readelf'))
  candidates.append(
    os.path.join(env.pnacl_base,
                 'pkg', 'binutils', 'bin', 'arm-pc-nacl-readelf'))

  # Otherwise, look for the system readelf
  for path in os.environ['PATH'].split(os.pathsep):
    candidates.append(os.path.join(path, 'readelf'))

  for readelf in candidates:
    if os.path.exists(readelf):
      return readelf

  Fatal('Cannot find readelf!')


def ReadELFInfo(f):
  ''' Returns: (arch, is_dynamic, is_glibc_static) '''

  readelf = env.readelf
  readelf_out = Run([readelf, '-lh', f], capture_stdout=True, verbose=False)

  machine_line = None
  is_dynamic = False
  is_glibc_static = False
  for line in readelf_out.split('\n'):
    line = line.strip()
    if line.startswith('Machine:'):
      machine_line = line
    if line.startswith('DYNAMIC'):
      is_dynamic = True
    if '__libc_atexit' in line:
      is_glibc_static = True

  if not machine_line:
    Fatal('Script error: readelf output did not make sense!')

  if 'Intel 80386' in machine_line:
    arch = 'x86-32'
  elif 'X86-64' in machine_line:
    arch = 'x86-64'
  elif 'ARM' in machine_line:
    arch = 'arm'
  elif 'MIPS' in machine_line:
    arch = 'mips32'
  else:
    Fatal('%s: Unknown machine type', f)

  return (arch, is_dynamic, is_glibc_static)


def GetSconsOS():
  name = platform.system().lower()
  if name == 'linux':
    return 'linux'
  if name == 'darwin':
    return 'mac'
  if 'cygwin' in name or 'windows' in name:
    return 'win'
  Fatal('Unsupported platform "%s"' % name)

def GetBuildArch():
  return ArchDict()[platform.machine()]


def FindBaseDir():
  '''Crawl backwards, starting from the directory containing this script,
     until we find the native_client/ directory.
  '''

  curdir = os.path.abspath(sys.argv[0])
  while os.path.basename(curdir) != 'native_client':
    curdir,subdir = os.path.split(curdir)
    if subdir == '':
      # We've hit the file system root
      break

  if os.path.basename(curdir) != 'native_client':
    Fatal('Unable to find native_client directory!')
  return curdir


if __name__ == '__main__':
  sys.exit(main(sys.argv))
