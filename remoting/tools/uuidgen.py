#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
uuidgen.py -- UUID generation utility.
"""

import sys
import uuid

def main():
  print uuid.uuid4()
  return 0

if __name__ == '__main__':
  sys.exit(main())
