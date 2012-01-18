#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import platform
import subprocess
import sys


class Environment:
  pass
env = Environment()


def Usage():
  name = sys.argv[0]
  print '-' * 80
  info = '''
%s [run_py_options] [sel_ldr_options] <nexe> [nexe_parameters]

run.py options:
  -L<LIBRARY_PATH>       Add a library path for runnable-ld.so
  --paranoid             Remove -S (signals) and -a (file access)
                         from the default sel_ldr options.

  --loader=dbg           Use dbg version of sel_ldr
  --loader=opt           Use opt version of sel_ldr
  --loader=<path>        Path to sel_ldr
  Default: Uses whichever sel_ldr already exists. Otherwise, builds opt version.

  --irt=full             Use full IRT
  --irt=core             Use Core IRT
  --irt=none             Don't use IRT
  --irt=<path>           Path to IRT nexe
  Default: Uses whichever IRT already exists. Otherwise, builds irt_core.

  -n | --dry-run         Just print commands, don't execute them
  -h | --help            Display this information
  -q | --quiet           Don't print nexe information
  --more                 Display sel_ldr usage

  -arch <arch> | -m32 | -m64 | -marm
                         Specify architecture for PNaCl translation
                         (arch is one of: x86-32, x86-64 or arm)

  --pnacl-glibc-dynamic  Translate a .pexe as dynamic against glibc
  --pnacl-glibc-static   Translate a .pexe as static against glibc
                         (These will be removed as soon as pnacl-translate
                          can auto-detect .pexe type)
'''
  print info % name
  print '-' * 80
  sys.exit(0)

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
  env.pnacl_root_newlib = os.path.join(env.nacl_root,
                                       'toolchain',
                                       'pnacl_%s_%s/newlib' % (GetBuildOS(),
                                                               GetBuildArch()))
  env.pnacl_root_glibc = os.path.join(env.nacl_root,
                                     'toolchain',
                                     'pnacl_%s_%s/glibc' % (GetBuildOS(),
                                                            GetBuildArch()))

  # QEMU
  env.arm_root = os.path.join(env.nacl_root,
                              'toolchain', 'linux_arm-trusted')
  env.qemu = os.path.join(env.arm_root, 'run_under_qemu_arm')

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

  # Don't print nexe information
  env.quiet = False

  # Arch (x86-32, x86-64, arm)
  env.arch = None

  # Translate a PNaCl file against glibc.
  # Valid values: 'static' or 'dynamic' or ''
  # This option will be removed as soon as pnacl-translate
  # can auto-detect .pexe type.
  env.pnacl_glibc = ''


def SetupEnvFlags():
  ''' Setup environment after config options have been read '''
  if env.pnacl_glibc != '':
    env.pnacl_root = env.pnacl_root_glibc
  else:
    env.pnacl_root = env.pnacl_root_newlib

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

def SetupArch(arch, is_dynamic, allow_build = True):
  '''Setup environment variables that require knowing the
     architecture. We can only do this after we've seen the
     nexe or once we've read -arch off the command-line.
  '''

  env.arch = arch

  if is_dynamic:
    if arch == 'x86-32':
      libdir = 'lib32'
    elif arch == 'x86-64':
      libdir = 'lib'
    else:
      Fatal('Dynamic loading is not yet supported on %s' % arch)
    env.runnable_ld = os.path.join(env.nnacl_root,
                                      'x86_64-nacl', libdir, 'runnable-ld.so')
    env.library_path.append(os.path.join(env.nnacl_root,
                                         'x86_64-nacl', libdir))

  env.sel_ldr = FindOrBuildSelLdr(allow_build = allow_build)
  env.irt = FindOrBuildIRT(allow_build = allow_build)


def main(argv):
  SetupEnvironment()

  sel_ldr_options, nexe, nexe_params = ArgSplit(argv[1:])

  SetupEnvFlags()
  # Translate .pexe files
  bypass_readelf = False
  if nexe.endswith('.pexe'):
    nexe = Translate(nexe)
    if env.dry_run:
      arch = env.arch
      # TODO(pdox): Pull this information from the pnacl driver
      # when it can be auto-detected.
      is_dynamic = (env.pnacl_glibc == 'dynamic')
      bypass_readelf = True

  # Read ELF Info
  if not bypass_readelf:
    (arch, is_dynamic, is_glibc_static) = ReadELFInfo(nexe)

  # Add default sel_ldr options
  if not env.paranoid:
    sel_ldr_options += ['-S', '-a']
    # X86-64 glibc static has validation problems without stub out (-s)
    if arch == 'x86-64' and is_glibc_static:
      sel_ldr_options += ['-s']
  if env.quiet:
    # Don't print sel_ldr logs
    sel_ldr_options += ['-l', '/dev/null']

  # Tell the user
  if is_dynamic:
    extra = 'DYNAMIC'
  else:
    extra = 'STATIC'
  PrintBanner('%s is %s %s' % (os.path.basename(nexe),
                               arch.upper(), extra))

  # Setup architecture-specific environment variables
  SetupArch(arch, is_dynamic)

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
  RunSelLdr(sel_ldr_args)
  return 0


def RunSelLdr(args):
  # TODO(pdox): If we are running this script on ARM, skip the emulator.
  prefix = []
  if env.arch == 'arm':
    prefix = [ env.qemu, '-cpu', 'cortex-a8']
    args = ['-Q'] + args

  # Use the bootstrap loader on linux.
  if env.scons_os == 'linux':
    bootstrap = os.path.join(os.path.dirname(env.sel_ldr),
                             'nacl_helper_bootstrap')
    loader = [bootstrap, env.sel_ldr]
  else:
    loader = [env.sel_ldr]
  Run(prefix + loader + args)


def FindOrBuildIRT(allow_build = True):
  if env.force_irt:
    if env.force_irt == 'none':
      return None
    elif env.force_irt == 'full':
      flavors = ['irt']
    elif env.force_irt == 'core':
      flavors = ['irt_core']
    else:
      irt = env.force_irt
      if not os.path.exists(irt):
        Fatal('IRT not found: %s' % irt)
      return irt
  else:
    flavors = ['irt','irt_core']

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

def FindOrBuildSelLdr(allow_build = True):
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


def Translate(pexe):
  arch = env.arch
  if arch is None:
    Fatal('Missing -arch for PNaCl translation.')
  translator = os.path.join(env.pnacl_root, 'bin', 'pnacl-translate')
  output_file = os.path.splitext(pexe)[0] + '.' + arch + '.nexe'
  args = [ translator, '-arch', arch, pexe, '-o', output_file ]
  if env.pnacl_glibc:
    args.append('--pnacl-use-glibc')
    if env.pnacl_glibc == 'static':
      args.append('-static')

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

def Run(args, cwd = None, capture = False):
  if not capture:
    PrintCommand(Stringify(args))
    if env.dry_run:
      return

  stdout_pipe = None
  stderr_pipe = None
  if capture:
    stdout_pipe = subprocess.PIPE
  if env.quiet and not env.paranoid:
    stderr_pipe = subprocess.PIPE

  p = None
  try:
    p = subprocess.Popen(args, stdout=stdout_pipe, stderr=stderr_pipe, cwd=cwd)
    (stdout_contents, stderr_contents) = p.communicate()
  except KeyboardInterrupt, e:
    if p:
      p.kill()
    raise e
  except BaseException, e:
    if p:
      p.kill()
    raise e

  if env.quiet and not env.paranoid:
    # Filter out sel_ldr's un-suppressable output when -a or -Q is given
    for line in stderr_contents.split('\n'):
      if (not line.startswith('DEBUG MODE') and
          not line.endswith('ACL CHECKS') and
          not line.startswith('PLATFORM QUALIFICATION') and
          line != ''):
        print >> sys.stderr, line

  if p.returncode != 0:
    if capture:
      Fatal('Failed to run: %s' % Stringify(args))
    sys.exit(p.returncode)

  return stdout_contents

def ArgSplit(argv):
  if len(argv) == 0:
    Usage()

  # Extract up to nexe
  sel_ldr_options = []
  nexe = None
  skip_one = False
  for i,arg in enumerate(argv):
    if skip_one:
      skip_one = False
      continue

    if arg.startswith('-L'):
      if arg == '-L':
        if i+1 < len(argv):
          path = argv[i+1]
          skip_one = True
        else:
          Fatal('Missing argument to -L')
      else:
        path = arg[len('-L'):]
      env.library_path.append(path)
    elif arg == '-m32':
      env.arch = 'x86-32'
    elif arg == '-m64':
      env.arch = 'x86-64'
    elif arg == '-marm':
      env.arch = 'arm'
    elif arg == '--pnacl-glibc-dynamic':
      env.pnacl_glibc = 'dynamic'
    elif arg == '--pnacl-glibc-static':
      env.pnacl_glibc = 'static'
    elif arg == '-arch':
      if i+1 < len(argv):
        env.arch = FixArch(argv[i+1])
        skip_one = True
    elif arg == '--paranoid':
      env.paranoid = True
    elif arg.startswith('--loader='):
      env.force_sel_ldr = arg[len('--loader='):]
    elif arg.startswith('--irt='):
      env.force_irt = arg[len('--irt='):]
    elif arg in ('-n', '--dry-run'):
      env.dry_run = True
    elif arg in ('-h', '--help'):
      Usage()
    elif arg in ('-q', '--quiet'):
      env.quiet = True
    elif arg in '--more':
      Usage2()
    elif arg.endswith('nexe') or arg.endswith('pexe'):
      nexe = arg
      break
    else:
      sel_ldr_options.append(arg)

  if not nexe:
    Fatal('No nexe given!')

  nexe_params = argv[i+1:]

  return sel_ldr_options, nexe, nexe_params


def FixArch(arch):
  x86_32 = 'x86-32 x86_32 x8632 i386 i686 ia32'.split()
  x86_64 = 'amd64 x86_64 x86-64 x8664'.split()
  arm = 'arm armv7'.split()

  if arch in x86_32:
    return 'x86-32'

  if arch in x86_64:
    return 'x86-64'

  if arch in arm:
    return 'arm'

  Fatal('Unrecognized arch "%s"!', arch)


def Fatal(msg, *args):
  if len(args) > 0:
    msg = msg % args
  print msg
  sys.exit(1)

def Usage2():
  # Try to find any sel_ldr that already exists
  for arch in ['x86-32','x86-64','arm']:
    SetupArch(arch, False, allow_build = False)
    if env.sel_ldr:
      break

  if not env.sel_ldr:
    # If nothing exists, build it.
    SetupArch('x86-32', False)

  RunSelLdr(['-h'])


def FindReadElf():
  '''Returns the path of "readelf" binary.'''

  # Use PNaCl's if it available.
  readelf = os.path.join(env.pnacl_root_newlib, 'bin', 'readelf')
  if os.path.exists(readelf):
    return readelf

  readelf = os.path.join(env.pnacl_root_glibc, 'bin', 'readelf')
  if os.path.exists(readelf):
    return readelf

  # Otherwise, look for the system readelf
  system_path = os.environ['PATH'].split(os.pathsep)
  for path in system_path:
    readelf = os.path.join(path, 'readelf')
    if os.path.exists(readelf):
      return readelf

  Fatal('Cannot find readelf!')


def ReadELFInfo(f):
  ''' Returns: (arch, is_dynamic, is_glibc_static) '''

  readelf = env.readelf
  readelf_out = Run([readelf, '-lh', f], capture = True)

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
  else:
    Fatal('%s: Unknown machine type', f)

  return (arch, is_dynamic, is_glibc_static)


def GetSconsOS():
  name = GetBuildOS()
  if name == 'linux':
    return 'linux'
  if name == 'darwin':
    return 'mac'
  if 'cygwin' in name or 'windows' in name:
    return 'win'
  Fatal('Unsupported platform "%s"' % name)


def GetBuildOS():
  name = platform.system().lower()
  if name not in ('linux', 'darwin'):
    Fatal('Unsupported platform "%s"' % (name,))
  return name

def GetBuildArch():
  return platform.machine()


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
