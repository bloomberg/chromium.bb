#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Regenerates all the .isolated test data files.

Keep in sync with ../run_isolated_smoke_test.py.
"""

import glob
import hashlib
import json
import os
import sys

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))


# Ordering is important to keep this script simple.
INCLUDES_TO_FIX = [
    ('manifest2.isolated', ['manifest1.isolated']),
    ('check_files.isolated', ['manifest2.isolated', 'repeated_files.isolated']),
]


def sha1(filename):
  with open(filename, 'rb') as f:
    return hashlib.sha1(f.read()).hexdigest()


def load(filename):
  with open(filename, 'r') as f:
    return json.load(f)


def save(filename, data):
  """Saves data as json properly formatted.

  Strips spurious whitespace json.dump likes to add at the end of lines and add
  a trailing \n.
  """
  out = ''.join(
      '%s\n' % l.rstrip()
      for l in json.dumps(data, indent=2, sort_keys=True).splitlines())
  with open(filename, 'wb') as f:
    f.write(out)


def main():
  # Simplify our life.
  os.chdir(ROOT_DIR)

  # First, reformat all the files.
  for filename in glob.glob('*.isolated'):
    save(filename, load(filename))

  # Then update the SHA-1s.
  for manifest, includes in INCLUDES_TO_FIX:
    data = load(manifest)
    data['includes'] = [sha1(f) for f in includes]
    save(manifest, data)
  return 0


if __name__ == '__main__':
  sys.exit(main())
