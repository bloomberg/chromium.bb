#!/usr/bin/env python

# Copyright (c) 2010 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

count = 0
try:
  count = open(sys.argv[1], 'r').read()
except:
  pass

open(sys.argv[1], 'w').write('%d' % (int(count) + 1))

sys.exit(0)
