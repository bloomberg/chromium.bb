#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys


def main():
  print 'Failing'
  return 1


if __name__ == '__main__':
  sys.exit(main())
