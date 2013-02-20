#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Tries to compile given code, produces different output depending on success.

This is similar to checks done by ./configure scripts.
"""


import optparse
import os
import shutil
import subprocess
import sys
import tempfile


def DoMain(argv):
  parser = optparse.OptionParser()
  parser.add_option('--code')
  parser.add_option('--on-success', default='')
  parser.add_option('--on-failure', default='')

  options, args = parser.parse_args(argv)

  if not options.code:
    parser.error('Missing required --code switch.')

  cxx = os.environ.get('CXX', 'g++')

  tmpdir = tempfile.mkdtemp()
  try:
    cxx_path = os.path.join(tmpdir, 'test.cc')
    with open(cxx_path, 'w') as f:
      f.write(options.code.decode('string-escape'))

    o_path = os.path.join(tmpdir, 'test.o')

    cxx_popen = subprocess.Popen([cxx, cxx_path, '-o', o_path, '-c'],
                                 stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE)
    cxx_stdout, cxx_stderr = cxx_popen.communicate()
    if cxx_popen.returncode == 0:
      print options.on_success
    else:
      print options.on_failure
  finally:
    shutil.rmtree(tmpdir)

  return 0


if __name__ == '__main__':
  sys.exit(DoMain(sys.argv[1:]))
