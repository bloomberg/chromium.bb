#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# $ unowned-flags.py chrome/browser/flag-metadata.json
# This program emits a list of all flags in the metadata file that have no
# owners entry, or whose owners entry is an empty list.

import os
import sys

PYJSON5_PATH = os.path.join(os.path.dirname(__file__),
                            '..', '..', 'third_party', 'pyjson5', 'src')
sys.path.append(PYJSON5_PATH)

import json5


def load_metadata(filename):
  return json5.load(open(filename))


def print_unowned_flags(filename):
  metadata = load_metadata(filename)
  for flag in metadata:
    if not flag.get('owners'):
      print flag['name']


if __name__ == '__main__':
  print_unowned_flags(sys.argv[1])
