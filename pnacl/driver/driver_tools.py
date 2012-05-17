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

import hashlib
import platform
import os
import re
import signal
import subprocess
import struct
import sys
import tempfile
import threading
import Queue

import artools
import ldtools
import pathtools

from driver_env import env
# TODO: import driver_log and change these references from 'foo' to
# 'driver_log.foo', or split driver_log further
from driver_log import Log, DriverOpen, DriverClose, StringifyCommand, TempFiles, DriverExit
from shelltools import shell

LLVM_BITCODE_MAGIC = 'BC\xc0\xde'
LLVM_WRAPPER_MAGIC = '\xde\xc0\x17\x0b'

def ParseError(s, leftpos, rightpos, msg):
  Log.Error("Parse Error: %s", msg);
  Log.Error('  ' + s);
  Log.Error('  ' + (' '*leftpos) + ('^'*(rightpos - leftpos + 1)))
  DriverExit(1)

# Run a command with extra environment settings
def RunWithEnv(cmd, **kwargs):
  RunWithLogArgs = { }
  if 'RunWithLogArgs' in kwargs:
    RunWithLogArgs = kwargs['RunWithLogArgs']
    del kwargs['RunWithLogArgs']

  env.push()
  env.setmany(**kwargs)
  ret = RunWithLog(cmd, **RunWithLogArgs)
  env.pop()
  return ret

def SetExecutableMode(path):
  if os.name == "posix":
    realpath = pathtools.tosys(path)
    # os.umask gets and sets at the same time.
    # There's no way to get it without setting it.
    umask = os.umask(0)
    os.umask(umask)
    os.chmod(realpath, 0755 & ~umask)

def RunDriver(invocation, args, suppress_arch = False):
  if isinstance(args, str):
    args = shell.split(env.eval(args))

  module_name = 'pnacl-%s' % invocation
  script = env.eval('${DRIVER_BIN}/%s' % module_name)
  script = shell.unescape(script)

  driver_args = env.get('DRIVER_FLAGS')

  if '--pnacl-driver-recurse' not in driver_args:
    driver_args.append('--pnacl-driver-recurse')

  # Get rid of -arch <arch> in the driver flags.
  if suppress_arch:
    while '-arch' in driver_args:
      i = driver_args.index('-arch')
      driver_args = driver_args[:i] + driver_args[i+2:]

  script = pathtools.tosys(script)
  cmd = [script] + driver_args + args

  # The invoked driver will do it's own logging, so
  # don't use RunWithLog() for the recursive driver call.
  # Use Run() so that the subprocess's stdout/stderr
  # will go directly to the real stdout/stderr.
  if env.getbool('DEBUG'):
    print '-' * 60
    print '\n' + StringifyCommand(cmd)

  module = __import__(module_name)
  # Save the environment, reset the environment, run
  # the driver module, and then restore the environment.
  env.push()
  env.reset()
  DriverMain(module, cmd)
  env.pop()

def memoize(f):
  """ Memoize a function with no arguments """
  saved = {}
  def newf():
    if len(saved) == 0:
      saved[None] = f()
    return saved[None]
  newf.__name__ = f.__name__
  return newf


@env.register
@memoize
def GetBuildOS():
  name = platform.system().lower()
  if name.startswith('cygwin_nt') or 'windows' in name:
    name = 'windows'
  if name not in ('linux', 'darwin', 'windows'):
    Log.Fatal("Unsupported platform '%s'", name)
  return name

@env.register
@memoize
def GetBuildArch():
  m = platform.machine()

  # Windows is special
  if m == 'x86':
    m = 'i686'

  if m not in ('i386', 'i686', 'x86_64'):
    Log.Fatal("Unsupported architecture '%s'", m)
  return m

# Crawl backwards, starting from the directory containing this script,
# until we find a directory satisfying a filter function.
def FindBaseDir(function):
  Depth = 0
  cur = env.getone('DRIVER_BIN')
  while not function(cur) and Depth < 16:
    cur = pathtools.dirname(cur)
    Depth += 1
  if function(cur):
    return cur
  return None

@env.register
@memoize
def FindBaseNaCl():
  """ Find native_client/ directory """
  dir = FindBaseDir(lambda cur: pathtools.basename(cur) == 'native_client')
  if dir is None:
    Log.Fatal("Unable to find 'native_client' directory")
  return shell.escape(dir)

@env.register
@memoize
def FindBaseToolchain():
  """ Find toolchain/ directory """
  dir = FindBaseDir(lambda cur: pathtools.basename(cur) == 'toolchain')
  if dir is None:
    Log.Fatal("Unable to find 'toolchain' directory")
  return shell.escape(dir)

@env.register
@memoize
def FindBasePNaCl():
  """ Find the base directory of the PNaCl toolchain """
  # The bin/ directory is one of:
  # <base>/bin if <base> is /path/to/pnacl_translator
  # <base>/newlib/bin or <base>/glibc/bin otherwise
  bindir = env.getone('DRIVER_BIN')

  # If this is a translator dir, use pnacl_translator
  translator_dir = FindBaseDir(
      lambda cur: pathtools.basename(cur) == 'pnacl_translator')
  if translator_dir is not None:
    return shell.escape(translator_dir)
  # Else use ../..
  basedir = pathtools.dirname(pathtools.dirname(bindir))
  return shell.escape(basedir)

def ReadConfig():
  driver_bin = env.getone('DRIVER_BIN')
  driver_conf = pathtools.join(driver_bin, 'driver.conf')
  fp = DriverOpen(driver_conf, 'r')
  linecount = 0
  for line in fp:
    linecount += 1
    line = line.strip()
    if line == '' or line.startswith('#'):
      continue
    sep = line.find('=')
    if sep < 0:
      Log.Fatal("%s: Parse error, missing '=' on line %d",
                pathtools.touser(driver_conf), linecount)
    keyname = line[:sep].strip()
    value = line[sep+1:].strip()
    env.setraw(keyname, value)
  DriverClose(fp)

  if env.getone('LIBMODE') not in ('newlib', 'glibc'):
    Log.Fatal('Invalid LIBMODE in %s', pathtools.touser(driver_conf))


@env.register
def AddPrefix(prefix, varname):
  values = env.get(varname)
  return ' '.join([prefix + shell.escape(v) for v in values ])

######################################################################
#
# Argument Parser
#
######################################################################

DriverArgPatterns = [
  ( '--pnacl-driver-verbose',             "env.set('LOG_VERBOSE', '1')"),
  ( '--pnacl-driver-log-to-file',         "env.set('LOG_TO_FILE', '1')"),
  ( '--pnacl-driver-debug',               "env.set('DEBUG', '1')"),
  ( '--pnacl-driver-recurse',             "env.set('RECURSE', '1')"),
  ( '--pnacl-driver-set-([^=]+)=(.*)',    "env.set($0, $1)"),
  ( '--pnacl-driver-append-([^=]+)=(.*)', "env.append($0, $1)"),
  ( ('-arch', '(.+)'),                 "SetArch($0)"),
  ( '--pnacl-sb',                      "env.set('SANDBOXED', '1')"),
  ( '--pnacl-sb-dynamic',              "env.set('SB_DYNAMIC', '1')"),
  ( '--pnacl-use-emulator',            "env.set('USE_EMULATOR', '1')"),
  ( '--dry-run',                       "env.set('DRY_RUN', '1')"),
  ( '--pnacl-arm-bias',                "env.set('BIAS', 'ARM')"),
  ( '--pnacl-i686-bias',               "env.set('BIAS', 'X8632')"),
  ( '--pnacl-x86_64-bias',             "env.set('BIAS', 'X8664')"),
  ( '--pnacl-bias=(.+)',               "env.set('BIAS', FixArch($0))"),
  ( '--pnacl-default-command-line',    "env.set('USE_DEFAULT_CMD_LINE', '1')"),
  ( '-save-temps',                     "env.set('SAVE_TEMPS', '1')"),
  ( '-no-save-temps',                  "env.set('SAVE_TEMPS', '0')"),
 ]


def ShouldExpandCommandFile(arg):
  """ On windows, we may be given files with commandline arguments.
  Read in the arguments so that they can be handled as usual. """
  if IsWindowsPython() and arg.startswith('@'):
    possible_file = pathtools.normalize(arg[1:])
    return pathtools.isfile(possible_file)
  else:
    return False

def DoExpandCommandFile(argv, i):
  arg = argv[i]
  fd = DriverOpen(pathtools.normalize(arg[1:]), 'r')
  more_args = fd.read().split()
  fd.close()
  new_argv = argv[:i] + more_args + argv[i+1:]
  return new_argv


def ParseArgs(argv, patternlist, must_match = True):
  """ Parse argv using the patterns in patternlist
      Returns: (matched, unmatched)
  """
  matched = []
  unmatched = []
  i = 0
  while i < len(argv):
    if ShouldExpandCommandFile(argv[i]):
      argv = DoExpandCommandFile(argv, i)
    num_matched, action, groups = MatchOne(argv, i, patternlist)
    if num_matched == 0:
      if must_match:
        Log.Fatal('Unrecognized argument: ' + argv[i])
      else:
        unmatched.append(argv[i])
      i += 1
      continue
    matched += argv[i:i+num_matched]
    if isinstance(action, str):
      # Perform $N substitution
      for g in xrange(0, len(groups)):
        action = action.replace('$%d' % g, 'groups[%d]' % g)
    try:
      if isinstance(action, str):
        # NOTE: this is essentially an eval for python expressions
        # which does rely on the current environment for unbound vars
        # Log.Info('about to exec [%s]', str(action))
        exec(action)
      else:
        action(*groups)
    except Exception, err:
      Log.Fatal('ParseArgs action [%s] failed with: %s', action, err)
    i += num_matched
  return (matched, unmatched)

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

def UnrecognizedOption(*args):
  Log.Fatal("Unrecognized option: " + ' '.join(args) + "\n" +
            "Use '--help' for more information.")

######################################################################
# File Type Tools
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
def IsNative(filename):
  return (IsNativeObject(filename) or
          IsNativeDSO(filename) or
          IsNativeArchive(filename))

@SimpleCache
def IsNativeObject(filename):
  return FileType(filename) == 'o'

@SimpleCache
def IsNativeDSO(filename):
  return FileType(filename) == 'so'

@SimpleCache
def IsBitcodeDSO(filename):
  return FileType(filename) == 'pso'

@SimpleCache
def IsBitcodeObject(filename):
  return FileType(filename) == 'po'

def IsBitcodeWrapperHeader(data):
  return data[:4] == LLVM_WRAPPER_MAGIC

@SimpleCache
def IsWrappedBitcode(filename):
  fp = DriverOpen(filename, 'rb')
  header = fp.read(4)
  iswbc = IsBitcodeWrapperHeader(header)
  DriverClose(fp)
  return iswbc

@SimpleCache
def IsBitcode(filename):
  fp = DriverOpen(filename, 'rb')
  header = fp.read(4)
  DriverClose(fp)
  # Raw bitcode
  if header == LLVM_BITCODE_MAGIC:
    return True
  # Wrapped bitcode
  return IsBitcodeWrapperHeader(header)

@SimpleCache
def IsArchive(filename):
  return artools.IsArchive(filename)

@SimpleCache
def IsBitcodeArchive(filename):
  filetype = FileType(filename)
  return filetype == 'archive-bc'

@SimpleCache
def IsNativeArchive(filename):
  return IsArchive(filename) and not IsBitcodeArchive(filename)

class ELFHeader(object):
  ELF_MAGIC = '\x7fELF'
  ELF_TYPES = { 1: 'REL',  # .o
                2: 'EXEC', # .exe
                3: 'DYN' } # .so
  ELF_MACHINES = {  3: '386',
                   40: 'ARM',
                   62: 'X86_64' }
  ELF_OSABI = { 0: 'UNIX',
                3: 'LINUX',
                123: 'NACL' }
  ELF_ABI_VER = { 0: 'NONE',
                  7: 'NACL' }

  def __init__(self, e_type, e_machine, e_osabi, e_abiver):
    self.type = self.ELF_TYPES[e_type]
    self.machine = self.ELF_MACHINES[e_machine]
    self.osabi = self.ELF_OSABI[e_osabi]
    self.abiver = self.ELF_ABI_VER[e_abiver]
    self.arch = FixArch(self.machine)  # For convenience

# If the file is not ELF, returns None.
# Otherwise, returns an ELFHeader object.
@SimpleCache
def GetELFHeader(filename):
  fp = DriverOpen(filename, 'rb')
  header = fp.read(16 + 2 + 2)
  DriverClose(fp)
  return DecodeELFHeader(header, filename)

def DecodeELFHeader(header, filename):
  # Pull e_ident, e_type, e_machine
  if header[0:4] != ELFHeader.ELF_MAGIC:
    return None

  e_osabi = DecodeLE(header[7])
  e_abiver = DecodeLE(header[8])
  e_type = DecodeLE(header[16:18])
  e_machine = DecodeLE(header[18:20])

  if e_osabi not in ELFHeader.ELF_OSABI:
    Log.Fatal('%s: ELF file has unknown OS ABI (%d)', filename, e_osabi)
  if e_abiver not in ELFHeader.ELF_ABI_VER:
    Log.Fatal('%s: ELF file has unknown ABI version (%d)', filename, e_abiver)
  if e_type not in ELFHeader.ELF_TYPES:
    Log.Fatal('%s: ELF file has unknown type (%d)', filename, e_type)
  if e_machine not in ELFHeader.ELF_MACHINES:
    Log.Fatal('%s: ELF file has unknown machine type (%d)', filename, e_machine)

  eh = ELFHeader(e_type, e_machine, e_osabi, e_abiver)
  return eh

def IsELF(filename):
  return GetELFHeader(filename) is not None

# Decode Little Endian bytes into an unsigned value
def DecodeLE(bytes):
  value = 0
  for b in reversed(bytes):
    value *= 2
    value += ord(b)
  return value

# Sandboxed LLC returns the metadata after translation. Use FORCED_METADATA
# if available instead of llvm-dis to avoid building llvm-dis on arm, and
# to make sure this interface gets tested
FORCED_METADATA = {}
@SimpleCache
def GetBitcodeMetadata(filename):
  assert(IsBitcode(filename))
  global FORCED_METADATA
  if filename in FORCED_METADATA:
    return FORCED_METADATA[filename]

  llvm_dis = env.getone('LLVM_DIS')
  args = [ llvm_dis, '-dump-metadata', filename ]
  _, stdout_contents, _ = Run(args, echo_stdout = False, return_stdout = True)

  metadata = { 'OutputFormat': '',
               'SOName'      : '',
               'NeedsLibrary': [] }
  for line in stdout_contents.split('\n'):
    if not line.strip():
      continue
    k, v = line.split(':')
    k = k.strip()
    v = v.strip()
    if k.startswith('NeededRecord_'):
      metadata[k] = ''
    assert(k in metadata)
    if isinstance(metadata[k], list):
      metadata[k].append(v)
    else:
      metadata[k] = v

  return metadata

def SetBitcodeMetadata(filename, is_shared, soname, needed_libs):
  metadata = {}
  metadata['OutputFormat'] = 'shared' if is_shared else 'executable'
  metadata['NeedsLibrary'] = list(needed_libs)
  metadata['SOName'] = soname
  global FORCED_METADATA
  FORCED_METADATA[filename] = metadata

# If FORCED_FILE_TYPE is set, FileType() will return FORCED_FILE_TYPE for all
# future input files. This is useful for the "as" incarnation, which
# needs to accept files of any extension and treat them as ".s" (or ".ll")
# files. Also useful for gcc's "-x", which causes all files to be treated
# in a certain way.
FORCED_FILE_TYPE = None
def SetForcedFileType(t):
  global FORCED_FILE_TYPE
  FORCED_FILE_TYPE = t

def GetForcedFileType():
  return FORCED_FILE_TYPE

def ForceFileType(filename, newtype = None):
  if newtype is None:
    if FORCED_FILE_TYPE is None:
      return
    newtype = FORCED_FILE_TYPE
  FileType.__cache[filename] = newtype

# File Extension -> Type string
# TODO(pdox): Add types for sources which should not be preprocessed.
ExtensionMap = {
  'c'   : 'c',

  'cc'  : 'c++',
  'cp'  : 'c++',
  'cxx' : 'c++',
  'cpp' : 'c++',
  'CPP' : 'c++',
  'c++' : 'c++',
  'C'   : 'c++',

  'm'   : 'objc',  # .m = "Objective-C source file"

  'll'  : 'll',
  'bc'  : 'po',
  'po'  : 'po',   # .po = "Portable object file"
  'pexe': 'pexe', # .pexe = "Portable executable"
  'pso' : 'pso',  # .pso = "Portable Shared Object"
  'asm' : 'S',
  'S'   : 'S',
  'sx'  : 'S',
  's'   : 's',
  'o'   : 'o',
  'os'  : 'o',
  'so'  : 'so',
  'nexe': 'nexe',
}

def IsSourceType(filetype):
  return filetype in ('c','c++','objc')

# The SimpleCache decorator is required for correctness, due to the
# ForceFileType mechanism.
@SimpleCache
def FileType(filename):
  # Auto-detect bitcode files, since we can't rely on extensions
  ext = filename.split('.')[-1]

  # TODO(pdox): We open and read the the first few bytes of each file
  #             up to 4 times, when we only need to do it once. The
  #             OS cache prevents us from hitting the disk, but this
  #             is still slower than it needs to be.
  if IsArchive(filename):
    return artools.GetArchiveType(filename)

  if IsELF(filename):
    return GetELFType(filename)

  if IsBitcode(filename):
    return GetBitcodeType(filename)

  if (ext in ('o','so','a','po','pso','pa','x') and
      ldtools.IsLinkerScript(filename)):
    return 'ldscript'

  # Use the file extension if it is recognized
  if ext in ExtensionMap:
    return ExtensionMap[ext]

  Log.Fatal('%s: Unrecognized file type', filename)


@SimpleCache
def GetELFType(filename):
  """ ELF type as determined by ELF metadata """
  assert(IsELF(filename))
  elfheader = GetELFHeader(filename)
  elf_type_map = {
    'EXEC': 'nexe',
    'REL' : 'o',
    'DYN' : 'so'
  }
  return elf_type_map[elfheader.type]

@SimpleCache
def GetBitcodeType(filename):
  """ Bitcode type as determined by bitcode metadata """
  assert(IsBitcode(filename))
  metadata = GetBitcodeMetadata(filename)
  format_map = {
    'object': 'po',
    'shared': 'pso',
    'executable': 'pexe'
  }
  return format_map[metadata['OutputFormat']]

def GetBasicHeaderData(data):
  """ Extract bitcode offset and size from LLVM bitcode wrapper header.
      Format is 4-bytes each of [ magic, version, bc offset, bc size, ...
      (documented at http://llvm.org/docs/BitCodeFormat.html)
  """
  if not IsBitcodeWrapperHeader(data):
    raise ValueError('Data is not a bitcode wrapper')
  magic, llvm_bcversion, offset, size = struct.unpack('<IIII', data)
  if llvm_bcversion != 0:
    raise ValueError('Data is not a valid bitcode wrapper')
  return offset, size

def WrapBitcode(output):
  """ Hash the bitcode and insert a wrapper header with the sha value.
      If the bitcode is already wrapped, the old hash is overwritten.
  """
  fd = DriverOpen(output, 'rb')
  maybe_header = fd.read(16)
  if IsBitcodeWrapperHeader(maybe_header):
    offset, bytes_left = GetBasicHeaderData(maybe_header)
    fd.seek(offset)
  else:
    offset = 0
    fd.seek(0, os.SEEK_END)
    bytes_left = fd.tell()
    fd.seek(0)
  # get the hash
  sha = hashlib.sha256()
  while bytes_left:
    block = fd.read(min(bytes_left, 4096))
    sha.update(block)
    bytes_left -= len(block)
  DriverClose(fd)
  # run bc-wrap
  retcode,_,_ = Run(' '.join(('${LLVM_BCWRAP}', '-hash',
                              sha.hexdigest(), output)))
  return retcode


######################################################################
#
# File Naming System (Temp files & Output files)
#
######################################################################

def DefaultOutputName(filename, outtype):
  if outtype in ('pp','dis'): return '-'; # stdout

  base = pathtools.basename(filename)
  base = RemoveExtension(base)
  if outtype in ('po'): return base + '.o'

  assert(outtype in ExtensionMap.values())
  assert(not IsSourceType(outtype))

  return base + '.' + outtype

def RemoveExtension(filename):
  if filename.endswith('.opt.bc'):
    return filename[0:-len('.opt.bc')]

  name, ext = pathtools.splitext(filename)
  return name

def PathSplit(f):
  paths = []
  cur = f
  while True:
    cur, piece = pathtools.split(cur)
    if piece == '':
      break
    paths.append(piece)
  paths.reverse()
  return paths

# Generate a unique identifier for each input file.
# Start with the basename, and if that is not unique enough,
# add parent directories. Rinse, repeat.
class TempNameGen(object):
  def __init__(self, inputs, output):
    inputs = [ pathtools.abspath(i) for i in inputs ]
    output = pathtools.abspath(output)

    self.TempBase = output + '---linked'

    # TODO(pdox): Figure out if there's a less confusing way
    #             to simplify the intermediate filename in this case.
    #if len(inputs) == 1:
    #  # There's only one input file, don't bother adding the source name.
    #  TempMap[inputs[0]] = output + '---'
    #  return

    # Build the initial mapping
    self.TempMap = dict()
    for f in inputs:
      if f.startswith('-'):
        continue
      path = PathSplit(f)
      self.TempMap[f] = [1, path]

    while True:
      # Find conflicts
      ConflictMap = dict()
      Conflicts = set()
      for (f, [n, path]) in self.TempMap.iteritems():
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
        n = self.TempMap[f][0]
        if n+1 > len(self.TempMap[f][1]):
          Log.Fatal('Unable to resolve naming conflicts')
        self.TempMap[f][0] = n+1

    # Clean up the map
    NewMap = dict()
    for (f, [n, path]) in self.TempMap.iteritems():
      candidate = output + '---' + '_'.join(path[-n:]) + '---'
      NewMap[f] = candidate
    self.TempMap = NewMap
    return

  def TempNameForOutput(self, imtype):
    temp = self.TempBase + '.' + imtype
    if not env.getbool('SAVE_TEMPS'):
      TempFiles.add(temp)
    return temp

  def TempNameForInput(self, input, imtype):
    fullpath = pathtools.abspath(input)
    # If input is already a temporary name, just change the extension
    if fullpath.startswith(self.TempBase):
      temp = self.TempBase + '.' + imtype
    else:
      # Source file
      temp = self.TempMap[fullpath] + '.' + imtype

    if not env.getbool('SAVE_TEMPS'):
      TempFiles.add(temp)
    return temp

# (Invoked from loader.py)
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
      try:
        p.kill()
      except BaseException:
        pass
    os.kill(os.getpid(), signal.SIGKILL)
    return 0
  if os.name == "posix":
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGHUP, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

def PipeRecord(sem, f, q):
  """ Read the output of a subprocess from the file object f one line at a
      time. Put each line on Queue q and release semaphore sem to wake the
      parent thread.
  """
  while True:
    line = f.readline()
    if line:
      q.put(line)
      sem.release()
    else:
      f.close()
      break
  return 0

def ProcessWait(sem, p):
  """ Wait for the subprocess.Popen object p to finish, and release
      the semaphore sem to wake the parent thread.
  """
  try:
    p.wait()
  except BaseException:
    pass
  sem.release()
  return 0

def QueueGetNext(q):
  """ Return the next line from Queue q, or None if empty.
  """
  try:
    nextline = q.get_nowait()
  except Queue.Empty:
    return None
  except KeyboardInterrupt as e:
    raise e
  else:
    return nextline

def RunWithLog(args, **kwargs):
  if env.getbool('LOGGING'):
    kwargs.setdefault('log_command', True)
    kwargs.setdefault('log_stdout', True)
    kwargs.setdefault('log_stderr', True)
  return Run(args, **kwargs)

def truecount(x, y, z):
  """ Count how many of the arguments are true """
  count = 0
  if x: count += 1
  if y: count += 1
  if z: count += 1
  return count

class RunConfig():
  """ Configuration for Run() function. """
  def __init__(self):
    self.stdin = None             # Contents for child's stdin (string)
    self.echo_stdout = True       # Echo the child's stdout to stdout
    self.echo_stderr = True       # Echo the child's stderr to stderr
    self.log_command = False      # Log the command being run
    self.log_stdout = False       # Log the child's stdout
    self.log_stderr = False       # Log the child's stderr
    self.errexit = True           # Exit on failure (errcode != 0)
    self.return_stdout = False    # Return the contents of stdout
    self.return_stderr = False    # Return the contents of stderr
    self.redirect_stdout = None   # Send stdout to a file object
    self.redirect_stderr = None   # Send stderr to a file object
    # These are properly set by SetupPipeConfig
    self.custom_stdout = False   # stdout needs CustomCommunicate
    self.custom_stderr = False   # stderr needs CustomCommunicate
    self.pipe_stdout = False      # pipe needs to be setup for stdout
    self.pipe_stderr = False      # pipe needs to be setup for stderr

  def SetupPipeConfig(self):
    # De-duplicate echo output in verbose mode
    if env.getbool('LOG_VERBOSE'):
      if self.log_stdout:
        self.echo_stdout = False
      if self.log_stderr:
        self.echo_stderr = False

    # If we only need to send the output to one place,
    # then we can do so directly using subprocess.communicate().
    #
    # If there are multiple destinations (e.g. echo & return),
    # we need to go through the multi-threaded processing
    # loop in CustomCommunicate().
    #
    # If logging is enabled, CustomCommunicate() is always used.
    self.custom_stdout = (self.log_stdout or
                          truecount(self.echo_stdout,
                                    self.return_stdout,
                                    self.redirect_stdout) > 1)
    self.custom_stderr = (self.log_stderr or
                          truecount(self.echo_stderr,
                                    self.return_stderr,
                                    self.redirect_stderr) > 1)
    self.pipe_stdout = self.custom_stdout or self.return_stdout
    self.pipe_stderr = self.custom_stderr or self.return_stderr

  def update(self, config_overrides):
    for k,v in config_overrides.iteritems():
      if not hasattr(self, k):
        Log.Fatal('Unknown parameter to Run(): %s = %s', k, v)
      setattr(self, k, v)


def ArgsTooLongForWindows(args):
  """ Detect when a command line might be too long for Windows.  """
  if not IsWindowsPython():
    return False
  else:
    return len(' '.join(args)) > 8191

def ConvertArgsToFile(args):
  fd, outfile = tempfile.mkstemp()
  # Remember to delete this file afterwards.
  TempFiles.add(outfile)
  cmd = args[0]
  other_args = args[1:]
  os.write(fd, ' '.join(other_args))
  os.close(fd)
  return [cmd, '@' + outfile]

def Run(args, **config_overrides):
  """ Run: Run a command.
      Returns: (exit code, stdout_contents, stderr_contents) """
  conf = RunConfig()
  conf.update(config_overrides)
  conf.SetupPipeConfig()

  if isinstance(args, str):
    args = shell.split(env.eval(args))

  args = [pathtools.tosys(args[0])] + args[1:]

  if conf.log_command:
    Log.Info('-' * 60)
    Log.Info('%s', StringifyCommand(args, conf.stdin))

  if env.getbool('DRY_RUN'):
    if conf.return_stdout or conf.return_stderr:
      # TODO(pdox): Prevent this from happening, so that
      # dry-run is more useful.
      Log.Fatal("Unhandled dry-run case.")
    return (0, '', '')

  if conf.stdin is not None:
    stdin_pipe = subprocess.PIPE
  else:
    stdin_pipe = None  # Inherit the parent's stdin

  if conf.pipe_stdout:
    stdout_pipe = subprocess.PIPE
  elif conf.redirect_stdout:
    stdout_pipe = conf.redirect_stdout
  elif conf.echo_stdout:
    stdout_pipe = None  # Inherit the parent's stdout
  else:
    stdout_pipe = open(os.devnull)

  if conf.pipe_stderr:
    stderr_pipe = subprocess.PIPE
  elif conf.redirect_stderr:
    stderr_pipe = conf.redirect_stderr
  elif conf.echo_stderr:
    stderr_pipe = None  # Inherit the parent's stderr
  else:
    stderr_pipe = open(os.devnull)

  try:
    # If we have too long of a cmdline on windows, running it would fail.
    # Attempt to use a file with the command line options instead in that case.
    if ArgsTooLongForWindows(args):
      actual_args = ConvertArgsToFile(args)
      if conf.log_command:
        Log.Info('Wrote long commandline to file for Windows: ' +
                 StringifyCommand(actual_args))

    else:
      actual_args = args
    p = subprocess.Popen(actual_args,
                         stdin=stdin_pipe,
                         stdout=stdout_pipe,
                         stderr=stderr_pipe)
  except Exception, e:
    msg =  '%s\nCommand was: %s' % (str(e), StringifyCommand(args, conf.stdin))
    if conf.log_command:
      Log.Fatal(msg)
    else:
      print msg
      DriverExit(1)
  CleanupProcesses.append(p)

  if conf.custom_stdout or conf.custom_stderr:
    (stdout_contents, stderr_contents) = CustomCommunicate(p, conf)
  else:
    (stdout_contents, stderr_contents) = p.communicate(input=conf.stdin)

  CleanupProcesses.pop()

  if conf.log_command:
    Log.Info('Return Code: ' + str(p.returncode))

  if conf.errexit and p.returncode != 0:
    DriverExit(p.returncode)

  return (p.returncode, stdout_contents, stderr_contents)

def CustomCommunicate(p, conf):
  """ Communicate w/ process 'p'.
      Send data to stdin, and process stdout/stderr.
      This is only used when logging is enabled or there are
      multiple output streams.
      Returns (stdout_contents, stderr_contents) """
  # CustomCommunicate exists so that data can be shuffled from the subprocess's
  # output streams (stdout and/or stderr) to one or more output streams in a
  # *non-blocking* manner.
  #
  # The non-blocking feature is key. We could use subprocess.communicate() in
  # these cases, but then we would be blocked while the program runs. There
  # would be no output in the meantime. This leads to a lags/stalls when
  # logging or verbose output is enabled and there is a large amount of debug
  # or error output.
  stdoutq = Queue.Queue()
  stderrq = Queue.Queue()
  IOReady = threading.Semaphore()
  threads = []

  # Python does not provide a portable way to do asynchronous socket handling.
  # So in order to process multiple streams simultaneously, we need to spawn
  # multiple threads.
  #
  # There are 4 threads maximum:
  #   Wait Thread  : Wait for the subprocess to die.
  #   Stdout Thread: Read stdout line-by-line, pushing to 'stdoutq'
  #   Stderr Thread: Read stderr line-by-line, pushing to 'stderrq'
  #   Main Thread  : Pop from stdoutq/stderrq, and send the data out.
  #
  # The stdout thread is only launched if stdout is piped.
  # The stderr thread is only launched if stderr is piped.
  #
  # The IOReady semaphore is used by the children threads to signal the main
  # thread when an event has occurred.
  wait_thread = threading.Thread(target=ProcessWait, args=(IOReady,p))
  threads.append(wait_thread)
  if conf.pipe_stdout:
    stdout_thread = threading.Thread(target=PipeRecord,
                                     args=(IOReady, p.stdout, stdoutq))
    threads.append(stdout_thread)
  if conf.pipe_stderr:
    stderr_thread = threading.Thread(target=PipeRecord,
                                     args=(IOReady, p.stderr, stderrq))
    threads.append(stderr_thread)

  for t in threads:
    t.start()

  if conf.stdin is not None:
    # This blocks while writing stdin.
    # TODO(pdox): Ideally, stdin would be fed in synchronously.
    p.stdin.write(conf.stdin)
    p.stdin.close()

  stdout_contents = ''
  stderr_contents = ''
  # Loop while handling I/O on stdout/stderr until the child finishes.
  # If custom_stderr/stdout are both false, then we just wait for the
  # ProcessWait thread
  lastio = False
  while True:
    IOReady.acquire()
    if p.poll() is not None:
      # Wait for the threads to finish so that the pipes are flushed.
      for t in threads:
        t.join()
      # The threads are now closed, but there might still
      # be data on the queue.
      lastio = True

    # For fair queueing, record the size here.
    stdout_qsize = stdoutq.qsize()
    stderr_qsize = stderrq.qsize()

    # Flush stdout queue
    while stdout_qsize > 0:
      line = QueueGetNext(stdoutq)
      if line:
        if conf.echo_stdout:
          sys.stdout.write(line)
        if conf.return_stdout:
          stdout_contents += line
        if conf.redirect_stdout:
          conf.redirect_stdout.write(line)
        if conf.log_stdout:
          Log.Info('STDOUT: ' + line.rstrip('\n'))
      stdout_qsize -= 1

    # Flush stderr queue
    while stderr_qsize > 0:
      line = QueueGetNext(stderrq)
      if line:
        if conf.echo_stderr:
          sys.stderr.write(line)
        if conf.return_stderr:
          stderr_contents += line
        if conf.redirect_stderr:
          conf.redirect_stderr.write(line)
        if conf.log_stderr:
          Log.Info('STDERR: ' + line.rstrip('\n'))
      stderr_qsize -= 1

    if lastio:
      break
  return (stdout_contents, stderr_contents)



def FixArch(arch):
  arch = arch.lower()
  archfix = { 'x86-32': 'X8632',
              'x86_32': 'X8632',
              'x8632' : 'X8632',
              'i686'  : 'X8632',
              'ia32'  : 'X8632',
              '386'   : 'X8632',
              '686'   : 'X8632',

              'amd64' : 'X8664',
              'x86_64': 'X8664',
              'x86-64': 'X8664',
              'x8664' : 'X8664',

              'arm'   : 'ARM',
              'armv7' : 'ARM',
              'arm-thumb2' : 'ARM' }
  if arch not in archfix:
    Log.Fatal('Unrecognized arch "%s"!', arch)
  return archfix[arch]

def IsWindowsPython():
  return 'windows' in platform.system().lower()

def SetupCygwinLibs():
  bindir = env.getone('DRIVER_BIN')
  os.environ['PATH'] += os.pathsep + pathtools.tosys(bindir)

# Map from GCC's -x file types and this driver's file types.
FILE_TYPE_MAP = {
    'c'                 : 'c',
    'c++'               : 'c++',
    'assembler'         : 's',
    'assembler-with-cpp': 'S',
}
FILE_TYPE_MAP_REVERSE = dict([reversed(_tmp) for _tmp in FILE_TYPE_MAP.items()])

def FileTypeToGCCType(filetype):
  return FILE_TYPE_MAP_REVERSE[filetype]

def GCCTypeToFileType(gcctype):
  if gcctype not in FILE_TYPE_MAP:
    Log.Fatal('language "%s" not recognized' % gcctype)
  return FILE_TYPE_MAP[gcctype]

def InitLog():
  Log.reset()
  Log.SetScriptName(env.getone('SCRIPT_NAME'))
  if env.getbool('LOG_VERBOSE'):
    env.set('LOGGING', '1')
    Log.LOG_OUT.append(sys.stderr)

  if env.getbool('LOG_TO_FILE'):
    env.set('LOGGING', '1')
    log_filename = env.getone('LOG_FILENAME')
    log_size_limit = int(env.getone('LOG_FILE_SIZE_LIMIT'))
    Log.AddFile(log_filename, log_size_limit)

def DriverMain(module, argv):
  # driver_path has the form: /foo/bar/pnacl_root/newlib/bin/pnacl-clang
  driver_path = pathtools.abspath(pathtools.normalize(argv[0]))
  driver_bin = pathtools.dirname(driver_path)
  script_name = pathtools.basename(driver_path)
  env.set('SCRIPT_NAME', script_name)
  env.set('DRIVER_PATH', driver_path)
  env.set('DRIVER_BIN', driver_bin)

  InitLog()
  ReadConfig()

  if IsWindowsPython():
    SetupCygwinLibs()

  # Parse driver arguments
  (driver_flags, main_args) = ParseArgs(argv[1:],
                                        DriverArgPatterns,
                                        must_match = False)
  env.append('DRIVER_FLAGS', *driver_flags)

  # Reinitialize the log, in case log settings were changed by
  # command-line arguments.
  InitLog()
  if not env.getbool('RECURSE'):
    Log.Banner(argv)

  # Handle help info
  help_func = getattr(module, 'get_help', None)
  if ('--help' in main_args or
      '-h' in main_args or
      '-help' in main_args or
      '--help-full' in main_args):
    if not help_func:
      Log.Fatal('Help text not available')
    helpstr = help_func(main_args)
    print helpstr
    return 0

  return module.main(main_args)

def SetArch(arch):
  env.set('ARCH', FixArch(arch))

def GetArch(required = False):
  arch = env.getone('ARCH')
  if arch == '':
    arch = None

  if required and not arch:
    Log.Fatal('Missing -arch!')

  return arch

# Read an ELF file to determine the machine type. If ARCH is already set,
# make sure the file has the same architecture. If ARCH is not set,
# set the ARCH to the file's architecture.
#
# Returns True if the file matches ARCH.
#
# Returns False if the file doesn't match ARCH. This only happens when
# must_match is False. If must_match is True, then a fatal error is generated
# instead.
def ArchMerge(filename, must_match):
  filetype = FileType(filename)
  if filetype in ('o','so'):
    elfheader = GetELFHeader(filename)
    if not elfheader:
      Log.Fatal("%s: Cannot read ELF header", filename)
    new_arch = elfheader.arch
  elif IsNativeArchive(filename):
    new_arch = filetype[len('archive-'):]
  else:
    Log.Fatal('%s: Unexpected file type in ArchMerge', filename)

  existing_arch = GetArch()

  if not existing_arch:
    SetArch(new_arch)
    return True
  elif new_arch != existing_arch:
    if must_match:
      msg = "%s: Incompatible object file (%s != %s)"
      logfunc = Log.Fatal
    else:
      msg = "%s: Skipping incompatible object file (%s != %s)"
      logfunc = Log.Warning
    logfunc(msg, filename, new_arch, existing_arch)
    return False
  else: # existing_arch and new_arch == existing_arch
    return True

def CheckTranslatorPrerequisites():
  """ Assert that the scons artifacts for running the sandboxed translator
      exist: sel_universal, sel_ldr and the irt blob. """
  for var in ['SEL_UNIVERSAL', 'SEL_LDR', 'BOOTSTRAP_LDR', 'IRT_BLOB']:
    needed_file = env.getone(var)
    if not pathtools.exists(needed_file):
      Log.Fatal('Could not find %s [%s]', var, needed_file)

class DriverChain(object):
  """ The DriverChain class takes one or more input files,
      an output file, and a sequence of steps. It executes
      those steps, using intermediate files in between,
      to generate the final outpu.
  """

  def __init__(self, input, output, namegen):
    self.input = input
    self.output = output
    self.steps = []
    self.namegen = namegen

    # "input" can be a list of files or a single file.
    # If we're compiling for a single file, then we use
    # TempNameForInput. If there are multiple files
    # (e.g. linking), then we use TempNameForOutput.
    self.use_names_for_input = isinstance(input, str)

  def add(self, callback, output_type, **extra):
    step = (callback, output_type, extra)
    self.steps.append(step)

  def run(self):
    step_input = self.input
    for (i, (callback, output_type, extra)) in enumerate(self.steps):
      if i == len(self.steps)-1:
        # Last step
        step_output = self.output
      else:
        # Intermediate step
        if self.use_names_for_input:
          step_output = self.namegen.TempNameForInput(self.input, output_type)
        else:
          step_output = self.namegen.TempNameForOutput(output_type)
      callback(step_input, step_output, **extra)
      step_input = step_output
