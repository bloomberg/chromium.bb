#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import idl_schema
from js_externs_generator import JsExternsGenerator
from datetime import datetime
import model
import unittest


# The contents of a fake idl file.
fake_idl = """
// Copyright %s The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A totally fake API.
namespace fakeApi {
  enum Greek {
    ALPHA,
    BETA,
    GAMMA,
    DELTA
  };

  dictionary Bar {
    long num;
  };

  dictionary Baz {
    DOMString str;
    long num;
    boolean b;
    Greek letter;
    Greek? optionalLetter;
    long[] arr;
    Bar[]? optionalObjArr;
    Greek[] enumArr;
    any[] anythingGoes;
    Bar obj;
    long? maybe;
    (DOMString or Greek or long[]) choice;
  };

  callback VoidCallback = void();

  callback BazGreekCallback = void(Baz baz, Greek greek);

  interface Functions {
    // Does something exciting! And what's more, this is a multiline function
    // comment! It goes onto multiple lines!
    // |baz| : The baz to use.
    static void doSomething(Baz baz, VoidCallback callback);

    // |callback| : The callback which will most assuredly in all cases be
    // called; that is, of course, iff such a callback was provided and is
    // not at all null.
    static void bazGreek(optional BazGreekCallback callback);

    [deprecated="Use a new method."] static DOMString returnString();
  };
};
"""

# The output we expect from our fake idl file.
expected_output = """// Copyright %s The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Externs generated from namespace: fakeApi */

/**
 * @const
 */
chrome.fakeApi = {};

/**
 * @enum {string}
 */
chrome.fakeApi.Greek = {
  ALPHA: 'ALPHA',
  BETA: 'BETA',
  GAMMA: 'GAMMA',
  DELTA: 'DELTA',
};

/**
 * @typedef {{
 *   num: number
 * }}
 */
var Bar;

/**
 * @typedef {{
 *   str: string,
 *   num: number,
 *   b: boolean,
 *   letter: !chrome.fakeApi.Greek,
 *   optionalLetter: (!chrome.fakeApi.Greek|undefined),
 *   arr: !Array<number>,
 *   optionalObjArr: (!Array<Bar>|undefined),
 *   enumArr: !Array<!chrome.fakeApi.Greek>,
 *   anythingGoes: !Array<*>,
 *   obj: Bar,
 *   maybe: (number|undefined),
 *   choice: (string|!chrome.fakeApi.Greek|!Array<number>)
 * }}
 */
var Baz;

/**
 * Does something exciting! And what's more, this is a multiline function
 * comment! It goes onto multiple lines!
 * @param {Baz} baz The baz to use.
 * @param {function():void} callback
 */
chrome.fakeApi.doSomething = function(baz, callback) {};

/**
 * @param {function(Baz, !chrome.fakeApi.Greek):void=} callback The callback
 *     which will most assuredly in all cases be called; that is, of course, iff
 *     such a callback was provided and is not at all null.
 */
chrome.fakeApi.bazGreek = function(callback) {};

/**
 * @return {string}
 * @deprecated Use a new method.
 */
chrome.fakeApi.returnString = function() {};
""" % datetime.now().year


class JsExternGeneratorTest(unittest.TestCase):
  def testBasic(self):
    self.maxDiff = None # Lets us see the full diff when inequal.
    filename = 'fake_api.idl'
    api_def = idl_schema.Process(fake_idl, filename)
    m = model.Model()
    namespace = m.AddNamespace(api_def[0], filename)
    self.assertMultiLineEqual(expected_output,
                              JsExternsGenerator().Generate(namespace).Render())


if __name__ == '__main__':
  unittest.main()
