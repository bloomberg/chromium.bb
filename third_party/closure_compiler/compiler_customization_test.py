#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from checker import Checker, FileCache, Flattener


CR_FILE = os.path.join("..", "..", "ui", "webui", "resources", "js", "cr.js")


def rel_to_abs(rel_path):
  script_path = os.path.dirname(os.path.abspath(__file__))
  return os.path.join(script_path, rel_path)


class CompilerCustomizationTest(unittest.TestCase):
  _CR_DEFINE_DEFINITION = Flattener(rel_to_abs(CR_FILE)).contents

  def setUp(self):
    self._checker = Checker()

  def _runCheckerTest(self, source_code, expected_error):
    file_path = "/script.js"
    FileCache._cache[file_path] = source_code
    _, output = self._checker.check(file_path)

    self.assertTrue(expected_error in output,
        msg="Expected chunk: \n%s\n\nOutput:\n%s\n" % (
            expected_error, output))

  def testGetInstance(self):
    self._runCheckerTest(source_code="""
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
""",
      expected_error="ERROR - actual parameter 1 of Class.needsNumber does "
                     "not match formal parameter")

  def testCrDefineFunctionDefinition(self):
    self._runCheckerTest(source_code=self._CR_DEFINE_DEFINITION + """
cr.define('a.b.c', function() {
  /** @param {number} num */
  function internalName(num) {}

  return {
    needsNumber: internalName
  };
});

a.b.c.needsNumber("wrong type");
""", expected_error="ERROR - actual parameter 1 of a.b.c.needsNumber does "
                    "not match formal parameter")

  def testCrDefineFunctionAssignment(self):
    self._runCheckerTest(source_code=self._CR_DEFINE_DEFINITION + """
cr.define('a.b.c', function() {
  /** @param {number} num */
  var internalName = function(num) {};

  return {
    needsNumber: internalName
  };
});

a.b.c.needsNumber("wrong type");
""", expected_error="ERROR - actual parameter 1 of a.b.c.needsNumber does "
                    "not match formal parameter")

  def testCrDefineConstructorDefinitionPrototypeMethod(self):
    self._runCheckerTest(source_code=self._CR_DEFINE_DEFINITION + """
cr.define('a.b.c', function() {
  /** @constructor */
  function ClassInternalName() {}

  ClassInternalName.prototype = {
    /** @param {number} num */
    method: function(num) {}
  };

  return {
    ClassExternalName: ClassInternalName
  };
});

new a.b.c.ClassExternalName().method("wrong type");
""", expected_error="ERROR - actual parameter 1 of a.b.c.ClassExternalName."
                    "prototype.method does not match formal parameter")

  def testCrDefineConstructorAssignmentPrototypeMethod(self):
    self._runCheckerTest(source_code=self._CR_DEFINE_DEFINITION + """
cr.define('a.b.c', function() {
  /** @constructor */
  var ClassInternalName = function() {};

  ClassInternalName.prototype = {
    /** @param {number} num */
    method: function(num) {}
  };

  return {
    ClassExternalName: ClassInternalName
  };
});

new a.b.c.ClassExternalName().method("wrong type");
""", expected_error="ERROR - actual parameter 1 of a.b.c.ClassExternalName."
                    "prototype.method does not match formal parameter")

  def testCrDefineEnum(self):
    self._runCheckerTest(source_code=self._CR_DEFINE_DEFINITION + """
cr.define('a.b.c', function() {
  /** @enum {string} */
  var internalNameForEnum = {key: 'wrong_type'};

  return {
    exportedEnum: internalNameForEnum
  };
});

/** @param {number} num */
function needsNumber(num) {}

needsNumber(a.b.c.exportedEnum.key);
""", expected_error="ERROR - actual parameter 1 of needsNumber does not "
                    "match formal parameter")


if __name__ == "__main__":
  unittest.main()
