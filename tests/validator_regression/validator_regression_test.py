#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import optparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time


KNOWN_BAD_NEXES = set(['naclapps/nexes/' + i for i in [
    ('kgegkcnohlcckapfehpogebcncidcdbe/'
     '1c2b051cb60367cf103128c9cd76769ffa1cf356.nexe'),
    ('noiedkpkpicokakliamgmmbmhkmkcdgj/'
     '1c2b051cb60367cf103128c9cd76769ffa1cf356.nexe'),
]])


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TESTS_DIR = os.path.dirname(SCRIPT_DIR)
NACL_DIR = os.path.dirname(TESTS_DIR)


# Imports from the build directory.
sys.path.insert(0, os.path.join(NACL_DIR, 'build'))
import download_utils


def NexeAritecture(filename):
  """Decide the architecture of a nexe.

  Args:
    filename: filename of the nexe.
  Returns:
    Architecture string (x86-32 / x86-64) or None.
  """
  fh = open(filename, 'rb')
  head = fh.read(20)
  # Must not be too short.
  if len(head) != 20:
    print 'ERROR - header too short'
    return None
  # Must have ELF header.
  if head[0:4] != '\x7fELF':
    print 'ERROR - no elf header'
    return None
  # Decode e_machine
  machine = struct.unpack('<H', head[18:])[0]
  return {
      3: 'x86-32',
      #40: 'arm',  # TODO(bradnelson): handle arm.
      62: 'x86-64',
  }.get(machine)


def GsutilCopySilent(src, dst):
  """Invoke gsutil cp, swallowing the output, with retry.

  Args:
    src: src url.
    dst: dst path.
  """
  for _ in range(3):
    env = os.environ.copy()
    env['PATH'] = '/b/build/scripts/slave' + os.pathsep + env['PATH']
    process = subprocess.Popen(
        ['gsutil', 'cp', src, dst],
        env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    process_stdout, process_stderr = process.communicate()
    if process.returncode == 0:
      return
  print 'Unexpected return code: %s' % process.returncode
  print '>>> STDOUT'
  print process_stdout
  print '>>> STDERR'
  print process_stderr
  print '-' * 70
  sys.exit(1)


def DownloadNexeList(filename):
  """Download list of NEXEs.

  Args:
    filename: destination filename.
  """
  GsutilCopySilent('gs://nativeclient-snaps/naclapps.list', filename)


def DownloadNexe(src_path, dst_filename):
  """Download list of NEXEs.

  Args:
    src_path: datastore relative path to download from.
    dst_filename: destination filename.
  """
  GsutilCopySilent('gs://nativeclient-snaps/%s' % src_path, dst_filename)


def PlatformInfo():
  """Determine info about the current platform.

  Returns:
    A tuple of nacl platform name and the exe suffix on this platform.
  """
  if sys.platform.startswith('linux'):
    platform = 'linux'
    platform_exe = ''
  elif sys.platform == 'darwin':
    platform = 'mac'
    platform_exe = ''
  elif sys.platform in ['win32', 'cygwin']:
    platform = 'win'
    platform_exe = '.exe'
  else:
    raise Exception('unsupported platform\n')
  return (platform, platform_exe)


def Sha1Sum(path):
  """Determine the sha1 hash of a file's contents given its path."""
  m = hashlib.sha1()
  fh = open(path, 'rb')
  m.update(fh.read())
  fh.close()
  return m.hexdigest()


def ValidateNexe(options, path, src_path, expect_pass):
  """Run the validator on a nexe, check if the result is expected.

  Args:
    options: bag of options.
    path: path to the nexe.
    src_path: path to nexe on server.
    expect_pass: boolean indicating if the nexe is expected to validate.
  """
  # Select which ncval to use.
  architecture = NexeAritecture(path)
  if architecture is None:
    print 'Validating: %s' % path
    print 'From: %s' % src_path
    print 'Size: %d' % os.path.getsize(path)
    print 'SHA1: %s' % Sha1Sum(path)
    raise Exception('Unknown nexe architecture')
  ncval = {
      'x86-32': options.ncval_x86_32,
      'x86-64': options.ncval_x86_64,
  }[architecture]
  # Run ncval.
  process = subprocess.Popen(
      [ncval, path],
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE)
  process_stdout, process_stderr = process.communicate()
  # Check if result is what we expect.
  did_pass = (process.returncode == 0)
  if expect_pass != did_pass:
    print '-' * 70
    print 'Validating: %s' % path
    print 'From: %s' % src_path
    print 'Size: %d' % os.path.getsize(path)
    print 'SHA1: %s' % Sha1Sum(path)
    print 'Unexpected return code: %s' % process.returncode
    print '>>> STDOUT'
    print process_stdout
    print '>>> STDERR'
    print process_stderr
    print '-' * 70
    sys.exit(1)


def NexeShouldValidate(path):
  """Checks a blacklist to decide if a nexe should validate.

  Args:
    path: path to the nexe.
  Returns:
    Boolean indicating if the nexe should validate.
  """
  return path not in KNOWN_BAD_NEXES



def CachedPath(options, filename):
  """Find the full path of a cached file, a cache root relative path.

  Args:
    options: bags of options.
    filename: filename relative to the top of the download url / cache.
  Returns:
    Absolute path of where the file goes in the cache.
  """
  return os.path.join(options.cache_dir, 'nacl_validator_test_cache', filename)


def Sha1FromFilename(filename):
  """Get the expected sha1 of a file path.

  Throughout we use the convention that files are store to a name of the form:
    <path_to_file>/<sha1hex>[.<some_extention>]
  This function extracts the expected sha1.

  Args:
    filename: filename to extract.
  Returns:
    Excepted sha1.
  """
  return os.path.splitext(os.path.basename(filename))[0]


def PrimeCache(options, filename):
  """Attempt to add a file to the cache directory if its not already there.

  Args:
    options: bag of options.
    filename: filename relative to the top of the download url / cache.
  """
  dpath = CachedPath(options, filename)
  if not os.path.exists(dpath) or Sha1Sum(dpath) != Sha1FromFilename(filename):
    # Try to make the directory, fail is ok, let the download fail instead.
    try:
      os.makedirs(os.path.basename(dpath))
    except OSError:
      pass
    DownloadNexe(filename, dpath)


def CopyFromCache(options, filename, dest_filename):
  """Copy an item from the cache.

  Args:
    options: bag of options.
    filename: filename relative to the top of the download url / cache.
    dest_filename: location to copy the file to.
  """
  dpath = CachedPath(options, filename)
  shutil.copy(dpath, dest_filename)
  assert Sha1Sum(dest_filename) == Sha1FromFilename(filename)


def TestValidators(options, work_dir):
  """Test x86 validators on current snapshot.

  Args:
    options: bag of options.
    work_dir: directory to operate in.
  """

  list_filename = os.path.join(work_dir, 'naclapps.list')
  nexe_filename = os.path.join(work_dir, 'test.nexe')
  DownloadNexeList(list_filename)
  fh = open(list_filename)
  filenames = fh.read().splitlines()
  fh.close()
  start = time.time()
  count = len(filenames)
  for index, filename in enumerate(filenames):
    tm = time.time()
    if index > 0:
      eta = (count - index) * (tm - start) / index
      eta_minutes = int(eta / 60)
      eta_seconds = int(eta - eta_minutes * 60)
      eta_str = ' (ETA %d:%02d)' % (eta_minutes, eta_seconds)
    else:
      eta_str = ''
    print 'Processing %d of %d%s...' % (index + 1, count, eta_str)
    PrimeCache(options, filename)
    # Stop here if downloading only.
    if options.download_only:
      continue
    # Validate a copy in case validator is mutating.
    CopyFromCache(options, filename, nexe_filename)
    try:
      ValidateNexe(
          options, nexe_filename, filename, NexeShouldValidate(filename))
    finally:
      try:
        os.remove(nexe_filename)
      except OSError:
        print 'ERROR - unable to remove %s' % nexe_filename
  print 'SUCCESS'


def Main():
  platform, platform_exe = PlatformInfo()
  ncval_x86_32 = os.path.join(
      NACL_DIR, 'scons-out',
      'opt-%s-x86-32' % platform,
      'staging', 'ncval' + platform_exe)
  ncval_x86_64 = os.path.join(
      NACL_DIR, 'scons-out',
      'opt-%s-x86-64' % platform,
      'staging', 'ncval' + platform_exe)

  # Decide a default cache directory.
  # Prefer /b (for the bots)
  # Failing that, use scons-out.
  # Failing that, use the current users's home dir.
  default_cache_dir = '/b'
  if not os.path.isdir(default_cache_dir):
    default_cache_dir = os.path.join(NACL_DIR, 'scons-out')
  if not os.path.isdir(default_cache_dir):
    default_cache_dir = os.path.expanduser('~/')
  default_cache_dir = os.path.abspath(default_cache_dir)
  assert os.path.isdir(default_cache_dir)

  parser = optparse.OptionParser()
  parser.add_option(
      '--cache-dir', dest='cache_dir', default=default_cache_dir,
      help='directory to cache downloads in')
  parser.add_option(
      '--download-only', default=False, action='store_true',
      help='download to cache without running the tests')
  parser.add_option(
      '--x86-32', dest='ncval_x86_32', default=ncval_x86_32,
      help='location of x86-32 validator')
  parser.add_option(
      '--x86-64', dest='ncval_x86_64', default=ncval_x86_64,
      help='location of x86-64 validator')
  options, args = parser.parse_args()
  if args:
    parser.error('unused arguments')

  work_dir = tempfile.mkdtemp(suffix='validate_nexes', prefix='tmp')
  try:
    TestValidators(options, work_dir)
  finally:
    download_utils.RemoveDir(work_dir)


if __name__ == '__main__':
  Main()
