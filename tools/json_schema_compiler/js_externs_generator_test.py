#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import idl_schema
import json_parse
from js_externs_generator import JsExternsGenerator
from datetime import datetime
import model
import unittest


# The contents of a fake idl file.
fake_idl = """
// Copyright 2014 The Chromium Authors. All rights reserved.
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

  interface Events {
    // Fired when we realize it's a trap!
    static void onTrapDetected(Baz baz);
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
 * @see https://developer.chrome.com/extensions/fakeApi#type-Greek
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
 * @see https://developer.chrome.com/extensions/fakeApi#type-Bar
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
 * @see https://developer.chrome.com/extensions/fakeApi#type-Baz
 */
var Baz;

/**
 * Does something exciting! And what's more, this is a multiline function
 * comment! It goes onto multiple lines!
 * @param {Baz} baz The baz to use.
 * @param {function():void} callback
 * @see https://developer.chrome.com/extensions/fakeApi#method-doSomething
 */
chrome.fakeApi.doSomething = function(baz, callback) {};

/**
 * @param {function(Baz, !chrome.fakeApi.Greek):void=} callback The callback
 *     which will most assuredly in all cases be called; that is, of course, iff
 *     such a callback was provided and is not at all null.
 * @see https://developer.chrome.com/extensions/fakeApi#method-bazGreek
 */
chrome.fakeApi.bazGreek = function(callback) {};

/**
 * @return {string}
 * @deprecated Use a new method.
 * @see https://developer.chrome.com/extensions/fakeApi#method-returnString
 */
chrome.fakeApi.returnString = function() {};

/**
 * Fired when we realize it's a trap!
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/fakeApi#event-onTrapDetected
 */
chrome.fakeApi.onTrapDetected;
""" % datetime.now().year


fake_json = """// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

[
  {
    "namespace": "fakeJson",
    "description": "Fake JSON API Stuff",
    "functions": [ {
      "name": "funcWithInlineObj",
      "type": "function",
      "parameters": [
        {
          "type": "object",
          "name": "inlineObj",
          "description": "Evil inline object! With a super duper duper long\
 string description that causes problems!",
          "properties": {
            "foo": {
              "type": "boolean",
              "optional": "true",
              "description": "The foo."
            },
            "bar": {
              "type": "integer",
              "description": "The bar."
            },
            "baz": {
              "type": "object",
              "description": "Inception object.",
              "properties": {
                "depth": {
                  "type": "integer"
                }
              }
            }
          }
        },
        {
          "name": "callback",
          "type": "function",
          "parameters": [
            {
              "type": "object",
              "name": "returnObj",
              "properties": {
                "str": { "type": "string"}
              }
            }
          ],
          "description": "The callback to this heinous method"
        }
      ],
      "returns": {
        "type": "object",
        "properties": {
          "str": { "type": "string" },
          "int": { "type": "number" }
        }
      }
    } ]
  }
]"""

json_expected = """// Copyright %s The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Externs generated from namespace: fakeJson */

/**
 * @const
 */
chrome.fakeJson = {};

/**
 * @param {{
 *   foo: (boolean|undefined),
 *   bar: number,
 *   baz: {
 *     depth: number
 *   }
 * }} inlineObj Evil inline object! With a super duper duper long string
 *     description that causes problems!
 * @param {function({
 *   str: string
 * }):void} callback The callback to this heinous method
 * @return {{
 *   str: string,
 *   int: number
 * }}
 * @see https://developer.chrome.com/extensions/fakeJson#method-funcWithInlineObj
 */
chrome.fakeJson.funcWithInlineObj = function(inlineObj, callback) {};
""" % datetime.now().year


class JsExternGeneratorTest(unittest.TestCase):
  def _GetNamespace(self, fake_content, filename, is_idl):
    """Returns a namespace object for the given content"""
    api_def = (idl_schema.Process(fake_content, filename) if is_idl
        else json_parse.Parse(fake_content))
    m = model.Model()
    return m.AddNamespace(api_def[0], filename)

  def setUp(self):
    self.maxDiff = None # Lets us see the full diff when inequal.

  def testBasic(self):
    namespace = self._GetNamespace(fake_idl, 'fake_api.idl', True)
    self.assertMultiLineEqual(expected_output,
                              JsExternsGenerator().Generate(namespace).Render())

  def testJsonWithInlineObjects(self):
    namespace = self._GetNamespace(fake_json, 'fake_api.json', False)
    self.assertMultiLineEqual(json_expected,
                              JsExternsGenerator().Generate(namespace).Render())


if __name__ == '__main__':
  unittest.main()
