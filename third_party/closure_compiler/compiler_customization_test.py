#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from checker import Checker
from processor import FileCache, Processor


ASSERT_FILE = os.path.join("..", "..", "ui", "webui", "resources", "js",
    "assert.js")
CR_FILE = os.path.join("..", "..", "ui", "webui", "resources", "js", "cr.js")
UI_FILE = os.path.join("..", "..", "ui", "webui", "resources", "js", "cr",
    "ui.js")


def rel_to_abs(rel_path):
  script_path = os.path.dirname(os.path.abspath(__file__))
  return os.path.join(script_path, rel_path)


class CompilerCustomizationTest(unittest.TestCase):
  _ASSERT_DEFINITION = Processor(rel_to_abs(ASSERT_FILE)).contents
  _CR_DEFINE_DEFINITION = Processor(rel_to_abs(CR_FILE)).contents
  _CR_UI_DECORATE_DEFINITION = Processor(rel_to_abs(UI_FILE)).contents

  def setUp(self):
    self._checker = Checker()

  def _runChecker(self, source_code):
    file_path = "/script.js"
    FileCache._cache[file_path] = source_code
    return self._checker.check(file_path)

  def _runCheckerTestExpectError(self, source_code, expected_error):
    _, output = self._runChecker(source_code)

    self.assertTrue(expected_error in output,
        msg="Expected chunk: \n%s\n\nOutput:\n%s\n" % (
            expected_error, output))

  def _runCheckerTestExpectSuccess(self, source_code):
    return_code, output = self._runChecker(source_code)

    self.assertTrue(return_code == 0,
        msg="Expected success, got return code %d\n\nOutput:\n%s\n" % (
            return_code, output))

  def testGetInstance(self):
    self._runCheckerTestExpectError("""
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
""", "ERROR - actual parameter 1 of Class.needsNumber does not match formal "
        "parameter")

  def testCrDefineFunctionDefinition(self):
    self._runCheckerTestExpectError(self._CR_DEFINE_DEFINITION + """
cr.define('a.b.c', function() {
  /** @param {number} num */
  function internalName(num) {}

  return {
    needsNumber: internalName
  };
});

a.b.c.needsNumber("wrong type");
""", "ERROR - actual parameter 1 of a.b.c.needsNumber does not match formal "
        "parameter")

  def testCrDefineFunctionAssignment(self):
    self._runCheckerTestExpectError(self._CR_DEFINE_DEFINITION + """
cr.define('a.b.c', function() {
  /** @param {number} num */
  var internalName = function(num) {};

  return {
    needsNumber: internalName
  };
});

a.b.c.needsNumber("wrong type");
""", "ERROR - actual parameter 1 of a.b.c.needsNumber does not match formal "
        "parameter")

  def testCrDefineConstructorDefinitionPrototypeMethod(self):
    self._runCheckerTestExpectError(self._CR_DEFINE_DEFINITION + """
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
""", "ERROR - actual parameter 1 of a.b.c.ClassExternalName.prototype.method "
        "does not match formal parameter")

  def testCrDefineConstructorAssignmentPrototypeMethod(self):
    self._runCheckerTestExpectError(self._CR_DEFINE_DEFINITION + """
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
""", "ERROR - actual parameter 1 of a.b.c.ClassExternalName.prototype.method "
        "does not match formal parameter")

  def testCrDefineEnum(self):
    self._runCheckerTestExpectError(self._CR_DEFINE_DEFINITION + """
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
""", "ERROR - actual parameter 1 of needsNumber does not match formal "
        "parameter")

  def testObjectDefineProperty(self):
    self._runCheckerTestExpectSuccess("""
/** @constructor */
function Class() {}

Object.defineProperty(Class.prototype, 'myProperty', {});

alert(new Class().myProperty);
""")

  def testCrDefineProperty(self):
    self._runCheckerTestExpectSuccess(self._CR_DEFINE_DEFINITION + """
/** @constructor */
function Class() {}

cr.defineProperty(Class.prototype, 'myProperty', cr.PropertyKind.JS);

alert(new Class().myProperty);
""")

  def testCrDefinePropertyTypeChecking(self):
    self._runCheckerTestExpectError(self._CR_DEFINE_DEFINITION + """
/** @constructor */
function Class() {}

cr.defineProperty(Class.prototype, 'booleanProp', cr.PropertyKind.BOOL_ATTR);

/** @param {number} num */
function needsNumber(num) {}

needsNumber(new Class().booleanProp);
""", "ERROR - actual parameter 1 of needsNumber does not match formal "
        "parameter")

  def testCrDefineOnCrWorks(self):
    self._runCheckerTestExpectSuccess(self._CR_DEFINE_DEFINITION + """
cr.define('cr', function() {
  return {};
});
""")

  def testAssertWorks(self):
    self._runCheckerTestExpectSuccess(self._ASSERT_DEFINITION + """
/** @return {?string} */
function f() {
  return "string";
}

/** @type {!string} */
var a = assert(f());
""")

  def testAssertInstanceofWorks(self):
    self._runCheckerTestExpectSuccess(self._ASSERT_DEFINITION + """
/** @constructor */
function Class() {}

/** @return {Class} */
function f() {
  var a = document.createElement('div');
  return assertInstanceof(a, Class);
}
""")

  def testCrUiDecorateWorks(self):
    self._runCheckerTestExpectSuccess(self._CR_DEFINE_DEFINITION +
        self._CR_UI_DECORATE_DEFINITION + """
/** @constructor */
function Class() {}

/** @return {Class} */
function f() {
  var a = document.createElement('div');
  cr.ui.decorate(a, Class);
  return a;
}
""")


if __name__ == "__main__":
  unittest.main()
