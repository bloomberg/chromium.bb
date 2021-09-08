# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script for ensuring that a python action runs under Python2, not Python3."""

import os
import subprocess
import sys


def find_python27():
  """Find python27 executable if available."""
  for path in os.environ.get('PATH', '').split(os.pathsep):
    for fname in ['python.exe', 'python2.exe', 'python27.exe']:
      fullpath = os.path.join(path, fname)
      if not os.path.exists(fullpath):
        continue
      p = subprocess.Popen(
        [
          fullpath,
          '-c',
          'import sys; print("%d.%d" % (sys.version_info.major, sys.version_info.minor))',
        ],
        encoding='utf-8',
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT)
      output = p.communicate()
      sys.stdout.flush()
      if 0 == p.returncode and '2.7' == output[0].strip():
        return fullpath
  raise Exception('Unable to find python27!')


if sys.version_info.major == 2:
  # If we get here, we're already Python2, so just re-execute the
  # command without the wrapper.
  exe = sys.executable
else:
  exe = find_python27()

sys.exit(subprocess.call([exe] + sys.argv[1:]))
