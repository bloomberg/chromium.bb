#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from checker import FileCache, Flattener, LineNumber


class NodeTest(unittest.TestCase):
  def __init__(self, *args, **kwargs):
    unittest.TestCase.__init__(self, *args, **kwargs)
    self.maxDiff = None

  def setUp(self):
    FileCache._cache["/debug.js"] = """
// Copyright 2002 Older Chromium Author dudes.
function debug(msg) { if (window.DEBUG) alert(msg); }
""".strip()

    FileCache._cache["/global.js"] = """
// Copyright 2014 Old Chromium Author dudes.
<include src="/debug.js">
var global = 'type checking!';
""".strip()

    FileCache._cache["/checked.js"] = """
// Copyright 2028 Future Chromium Author dudes.
/**
 * @fileoverview Coolest app ever.
 * @author Douglas Crockford (douglas@crockford.com)
 */
<include src="/global.js">
debug(global);
// Here continues checked.js, a swell file.
""".strip()

    self.flattener_ = Flattener("/checked.js")

  def testInline(self):
    self.assertMultiLineEqual("""
// Copyright 2028 Future Chromium Author dudes.
/**
 * @fileoverview Coolest app ever.
 * @author Douglas Crockford (douglas@crockford.com)
 */
// Copyright 2014 Old Chromium Author dudes.
// Copyright 2002 Older Chromium Author dudes.
function debug(msg) { if (window.DEBUG) alert(msg); }
var global = 'type checking!';
debug(global);
// Here continues checked.js, a swell file.
""".strip(), self.flattener_.contents)

  def assertLineNumber(self, abs_line, expected_line):
    actual_line = self.flattener_.get_file_from_line(abs_line)
    self.assertEqual(expected_line.file, actual_line.file)
    self.assertEqual(expected_line.line_number, actual_line.line_number)

  def testGetFileFromLine(self):
    self.assertLineNumber(1, LineNumber("/checked.js", 1))
    self.assertLineNumber(5, LineNumber("/checked.js", 5))
    self.assertLineNumber(6, LineNumber("/global.js", 1))
    self.assertLineNumber(7, LineNumber("/debug.js", 1))
    self.assertLineNumber(8, LineNumber("/debug.js", 2))
    self.assertLineNumber(9, LineNumber("/global.js", 3))
    self.assertLineNumber(10, LineNumber("/checked.js", 7))
    self.assertLineNumber(11, LineNumber("/checked.js", 8))


if __name__ == '__main__':
  unittest.main()
