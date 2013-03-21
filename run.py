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

Run a command-line nexe (or pexe). Automatically handles translation,
building sel_ldr, and building the IRT.

run.py options:
  -L<LIBRARY_PATH>       Additional library path for runnable-ld.so
  --paranoid             Remove -S (signals) and -a (file access)
                         from the default sel_ldr options.

  --loader=dbg           Use dbg version of sel_ldr
  --loader=opt           Use opt version of sel_ldr
  --loader=<path>        Path to sel_ldr
  Default: Uses whichever sel_ldr already exists. Otherwise, builds opt version.

  --irt=core             Use Core IRT
  --irt=none             Don't use IRT
  --irt=<path>           Path to IRT nexe
  Default: Uses whichever IRT already exists. Otherwise, builds irt_core.

  -n | --dry-run         Just print commands, don't execute them
  -h | --help            Display this information
  -q | --quiet           Don't print anything (nexe output can be redirected
                         with NACL_EXE_STDOUT/STDERR env vars)
  --more                 Display sel_ldr usage

  -arch <arch> | -m32 | -m64 | -marm | -mmips32
                         Specify architecture for PNaCl translation
                         (arch is one of: x86-32, x86-64, arm or mips32)
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
  pnacl_label = 'pnacl_%s_%s' % (GetSconsOS(), GetBuildArch().replace('-','_'))
  env.pnacl_base = os.path.join(env.nacl_root, 'toolchain', pnacl_label)
  env.pnacl_root_newlib = os.path.join(env.pnacl_base, 'newlib')
  env.pnacl_root_glibc = os.path.join(env.pnacl_base, 'glibc')

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

def SetupArch(arch, allow_build = True):
  '''Setup environment variables that require knowing the
     architecture. We can only do this after we've seen the
     nexe or once we've read -arch off the command-line.
  '''
  env.arch = arch
  env.sel_ldr = FindOrBuildSelLdr(allow_build = allow_build)
  env.irt = FindOrBuildIRT(allow_build = allow_build)


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

  sel_ldr_options, nexe, nexe_params = ArgSplit(argv[1:])

  # Translate .pexe files
  is_pnacl = nexe.endswith('.pexe')
  if is_pnacl:
    nexe, metadata = Translate(env.arch, nexe)
    if metadata['OutputFormat'] != 'executable':
      Fatal('Bitcode has non-executable type: %s', metadata['OutputFormat'])

  # Read the ELF file info
  if is_pnacl and env.dry_run:
    # In a dry run, we don't actually run pnacl-translate, so there is
    # no nexe for readelf. Fill in the information manually.
    has_needed = len(metadata['NeedsLibrary']) > 0
    arch, is_dynamic, is_glibc_static = env.arch, has_needed, False
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
    sel_ldr_options += ['-l', '/dev/null']
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
  RunSelLdr(sel_ldr_args)
  return 0


def RunSelLdr(args):
  prefix = []
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
    args = ['--r_debug=0x' + template_digits,
            '--reserved_at_zero=0x' + template_digits] + args
  else:
    loader = [env.sel_ldr]
  Run(prefix + loader + args)


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

def Translate(arch, pexe):
  if arch is None:
    Fatal('Missing -arch for PNaCl translation.')
  metadata = GetBitcodeMetadata(pexe)
  output_file = os.path.splitext(pexe)[0] + '.' + arch + '.nexe'
  # TODO(pdox): It shouldn't be necessary to branch here.
  # Both newlib and glibc's pnacl-translate should behave identically.
  # BUG= http://code.google.com/p/nativeclient/issues/detail?id=2423
  has_needed = len(metadata['NeedsLibrary']) > 0
  if has_needed:
    rootdir = env.pnacl_root_glibc
  else:
    rootdir = env.pnacl_root_newlib

  pnacl_translate = os.path.join(rootdir, 'bin', 'pnacl-translate')
  args = [ pnacl_translate, '-arch', arch, pexe, '-o', output_file ]
  Run(args)
  return output_file, metadata

def GetBitcodeMetadata(pexe):
  pnaclmeta = os.path.join(env.pnacl_root_newlib, 'bin', 'pnacl-meta')
  args = [ pnaclmeta, '--raw', pexe ]
  raw_metadata = Run(args, capture = True)
  metadata = { 'OutputFormat': '',
               'SOName'      : '',
               'NeedsLibrary': [] }
  for line in raw_metadata.split('\n'):
    line = line.strip()
    if not line:
      continue
    k, v = line.split(':')
    k = k.strip()
    v = v.strip()
    if isinstance(metadata[k], list):
      metadata[k].append(v)
    else:
      metadata[k] = v
  return metadata

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
  stderr_redir = None
  if capture:
    stdout_pipe = subprocess.PIPE
  if env.quiet and not env.paranoid:
    # Even if you redirect NACLLOG to /dev/null it still prints
    # "DEBUG MODE ENABLED (bypass acl)", so just swallow the output
    stderr_redir = open(os.devnull)
    os.environ['NACLLOG'] = os.devnull

  p = None
  try:
    p = subprocess.Popen(args, stdout=stdout_pipe, stderr=stderr_redir, cwd=cwd)
    (stdout_contents, stderr_contents) = p.communicate()
  except KeyboardInterrupt, e:
    if p:
      p.kill()
    raise e
  except BaseException, e:
    if p:
      p.kill()
    raise e

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
    elif arg == '-mmips32':
      env.arch = 'mips32'
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
    elif arg in ('-t', '--trace'):
      env.trace = True
    elif arg in ('-g', '--debug'):
      env.debug = True
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
  mips32 = 'mips mips32'.split()

  if arch in x86_32:
    return 'x86-32'

  if arch in x86_64:
    return 'x86-64'

  if arch in arm:
    return 'arm'

  if arch in mips32:
    return 'mips32'

  Fatal('Unrecognized arch "%s"!', arch)


def Fatal(msg, *args):
  if len(args) > 0:
    msg = msg % args
  print msg
  sys.exit(1)

def Usage2():
  # Try to find any sel_ldr that already exists
  for arch in ['x86-32','x86-64','arm','mips32']:
    SetupArch(arch, allow_build = False)
    if env.sel_ldr:
      break

  if not env.sel_ldr:
    # If nothing exists, build it.
    SetupArch('x86-32')

  RunSelLdr(['-h'])


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
  return FixArch(platform.machine())


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
