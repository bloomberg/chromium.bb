#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import isolate


class Isolate(unittest.TestCase):
  def test_load_empty(self):
    content = "{}"
    variables = {}
    command, infiles, read_only = isolate.load_isolate(
        content, variables, self.fail)
    self.assertEquals([], command)
    self.assertEquals([], infiles)
    self.assertEquals(None, read_only)


if __name__ == '__main__':
  unittest.main()
