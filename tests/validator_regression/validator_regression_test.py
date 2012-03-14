#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import os
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


def ValidateNexe(path, src_path, expect_pass):
  """Run the validator on a nexe, check if the result is expected.

  Args:
    path: path to the nexe.
    src_path: path to nexe on server.
    expect_pass: boolean indicating if the nexe is expected to validate.
  """
  # Select which ncval to use.
  platform, platform_exe = PlatformInfo()
  architecture = NexeAritecture(path)
  if architecture is None:
    print 'Validating: %s' % path
    print 'From: %s' % src_path
    print 'Size: %d' % os.path.getsize(path)
    print 'SHA1: %s' % Sha1Sum(path)
    raise Exception('Unknown nexe architecture')
  ncval = os.path.join(
      NACL_DIR, 'scons-out',
      'opt-%s-%s' % (platform, architecture),
      'staging', 'ncval' + platform_exe)
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


def TestValidators(work_dir):
  """Test x86 validators on current snapshot.

  Args:
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
    print 'Checking %d of %d%s...' % (index + 1, count, eta_str)
    DownloadNexe(filename, nexe_filename)
    ValidateNexe(nexe_filename, filename, NexeShouldValidate(filename))
  print 'SUCCESS'


def Main():
  work_dir = tempfile.mkdtemp(suffix='validate_nexes', prefix='tmp')
  try:
    TestValidators(work_dir)
  finally:
    download_utils.RemoveDir(work_dir)


if __name__ == '__main__':
  Main()
