#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from checker import Checker, FileCache


class CodingConventionTest(unittest.TestCase):
  def setUp(self):
    self._checker = Checker(verbose=False)

  def testGetInstance(self):
    file_content = """
var cr = {
  /** @param {!Function} ctor */
  addSingletonGetter: function(ctor) {
    ctor.getInstance = function() {
      return ctor.instance_ || (ctor.instance_ = new ctor());
    };
  }
};

/** @constructor */
function Class() {
  /** @param {number} num */
  this.needsNumber = function(num) {};
}

cr.addSingletonGetter(Class);
Class.getInstance().needsNumber("wrong type");
"""
    expected_chunk = "ERROR - actual parameter 1 of Class.needsNumber does not match formal parameter"

    file_path = "/script.js"
    FileCache._cache[file_path] = file_content
    _, output = self._checker.check(file_path)

    self.assertTrue(expected_chunk in output,
        msg="%s\n\nExpected chunk: \n%s\n\nOutput:\n%s\n" % (
            "getInstance", expected_chunk, output))


if __name__ == "__main__":
  unittest.main()
