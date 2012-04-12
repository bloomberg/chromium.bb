#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cStringIO
import hashlib
import json
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

import isolate

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
VERBOSE = False



class CalledProcessError(subprocess.CalledProcessError):
  """Makes 2.6 version act like 2.7"""
  def __init__(self, returncode, cmd, output, cwd):
    super(CalledProcessError, self).__init__(returncode, cmd)
    self.output = output
    self.cwd = cwd

  def __str__(self):
    return super(CalledProcessError, self).__str__() + (
        '\n'
        'cwd=%s\n%s') % (self.cwd, self.output)


class Isolate(unittest.TestCase):
  def setUp(self):
    # The tests assume the current directory is the file's directory.
    os.chdir(ROOT_DIR)
    self.tempdir = tempfile.mkdtemp()
    self.result = os.path.join(self.tempdir, 'result')
    self.child = os.path.join('data', 'isolate', 'child.py')
    if VERBOSE:
      print
    self.files = [
      self.child,
      os.path.join('data', 'isolate', 'files1', 'test_file1.txt'),
      os.path.join('data', 'isolate', 'files1', 'test_file2.txt'),
    ]

  def tearDown(self):
    shutil.rmtree(self.tempdir)

  def _expected_tree(self, files):
    self.assertEquals(sorted(files), sorted(os.listdir(self.tempdir)))

  def _expected_result(self, with_hash, files, args, read_only):
    if sys.platform == 'win32':
      mode = lambda _: 420
    else:
      # 4 modes are supported, 0755 (rwx), 0644 (rw), 0555 (rx), 0444 (r)
      min_mode = 0444
      if not read_only:
        min_mode |= 0200
      def mode(filename):
        return (min_mode | 0111) if filename.endswith('.py') else min_mode

    if not isinstance(files, dict):
      # Update files to dict.
      files = dict((unicode(f), {u'mode': mode(f)}) for f in files)
    # Add size and timestamp.
    files = files.copy()
    for k, v in files.iteritems():
      if v:
        filestats = os.stat(k)
        v[u'size'] = filestats.st_size
        # Used the skip recalculating the hash. Use the most recent update
        # time.
        v[u'timestamp'] = int(round(
            max(filestats.st_mtime, filestats.st_ctime)))

    expected = {
      u'files': files,
      u'relative_cwd': u'data/isolate',
      u'read_only': None,
    }
    if args:
      expected[u'command'] = [u'python'] + [unicode(x) for x in args]
    else:
      expected[u'command'] = []
    if with_hash:
      for filename in expected[u'files']:
        # Calculate our hash.
        h = hashlib.sha1()
        h.update(open(os.path.join(ROOT_DIR, filename), 'rb').read())
        expected[u'files'][filename][u'sha-1'] = unicode(h.hexdigest())

    actual = json.load(open(self.result, 'rb'))
    self.assertEquals(expected, actual)
    return expected

  def _execute(self, filename, args, need_output=False):
    cmd = [
      sys.executable, os.path.join(ROOT_DIR, 'isolate.py'),
      '--variable', 'DEPTH=%s' % ROOT_DIR,
      '--result', self.result,
      os.path.join(ROOT_DIR, 'data', 'isolate', filename),
    ] + args
    env = os.environ.copy()
    if 'ISOLATE_DEBUG' in env:
      del env['ISOLATE_DEBUG']
    if need_output or not VERBOSE:
      stdout = subprocess.PIPE
      stderr = subprocess.STDOUT
    else:
      cmd.extend(['-v'] * 3)
      stdout = None
      stderr = None
    cwd = ROOT_DIR
    p = subprocess.Popen(
        cmd + args,
        stdout=stdout,
        stderr=stderr,
        cwd=cwd,
        env=env,
        universal_newlines=True)
    out = p.communicate()[0]
    if p.returncode:
      raise CalledProcessError(p.returncode, cmd, out, cwd)
    return out

  def test_help_modes(self):
    # Check coherency in the help and implemented modes.
    p = subprocess.Popen(
        [sys.executable, os.path.join(ROOT_DIR, 'isolate.py'), '--help'],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        cwd=ROOT_DIR)
    out = p.communicate()[0].splitlines()
    self.assertEquals(0, p.returncode)
    out = out[out.index('') + 1:]
    out = out[:out.index('')]
    modes = [re.match(r'^  (\w+) .+', l) for l in out]
    modes = tuple(m.group(1) for m in modes if m)
    # Keep the list hard coded.
    expected = ('check', 'hashtable', 'remap', 'run', 'trace')
    self.assertEquals(expected, modes)
    self.assertEquals(expected, modes)
    for mode in modes:
      self.assertTrue(hasattr(self, 'test_%s' % mode), mode)
    self._expected_tree([])

  def test_check(self):
    self._execute('fail.isolate', ['--mode', 'check'])
    self._expected_tree(['result'])
    self._expected_result(
        False, dict((f, {}) for f in self.files), ['child.py', '--fail'], False)

  def test_check_no_run(self):
    self._execute('no_run.isolate', ['--mode', 'check'])
    self._expected_tree(['result'])
    self._expected_result(
        False, dict((f, {}) for f in self.files), None, False)

  def test_check_non_existent(self):
    try:
      self._execute('non_existent.isolate', ['--mode', 'check'])
      self.fail()
    except subprocess.CalledProcessError:
      pass
    self._expected_tree([])

  def test_check_directory_no_slash(self):
    try:
      self._execute('missing_trailing_slash.isolate', ['--mode', 'check'])
      self.fail()
    except subprocess.CalledProcessError:
      pass
    self._expected_tree([])

  def test_hashtable(self):
    cmd = [
      '--mode', 'hashtable',
      '--outdir', self.tempdir,
    ]
    self._execute('no_run.isolate', cmd)
    data = self._expected_result(True, self.files, None, False)
    self._expected_tree(
        [f['sha-1'] for f in data['files'].itervalues()] + ['result'])

  def test_remap(self):
    cmd = [
      '--mode', 'remap',
      '--outdir', self.tempdir,
    ]
    self._execute('no_run.isolate', cmd)
    self._expected_tree(['data', 'result'])
    self._expected_result(
        False,
        self.files,
        None,
        False)

  def test_run(self):
    self._execute('ok.isolate', ['--mode', 'run'])
    self._expected_tree(['result'])
    # cmd[0] is not generated from infiles[0] so it's not using a relative path.
    self._expected_result(
        False, self.files, ['child.py', '--ok'], False)

  def test_run_fail(self):
    try:
      self._execute('fail.isolate', ['--mode', 'run'])
      self.fail()
    except subprocess.CalledProcessError:
      pass
    self._expected_tree(['result'])

  def test_trace(self):
    out = self._execute('ok.isolate', ['--mode', 'trace'], True)
    self._expected_tree(['result', 'result.log'])
    # The 'result.log' log is OS-specific so we can't read it but we can read
    # the gyp result.
    # cmd[0] is not generated from infiles[0] so it's not using a relative path.
    self._expected_result(
        False, self.files, ['child.py', '--ok'], False)

    expected_value = {
      'conditions': [
        ['OS=="%s"' % isolate.trace_inputs.get_flavor(), {
          'variables': {
            isolate.trace_inputs.KEY_TRACKED: [
              'child.py',
            ],
            isolate.trace_inputs.KEY_UNTRACKED: [
              'files1/',
            ],
          },
        }],
      ],
    }
    expected_buffer = cStringIO.StringIO()
    isolate.trace_inputs.pretty_print(expected_value, expected_buffer)
    self.assertEquals(expected_buffer.getvalue(), out)



if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
