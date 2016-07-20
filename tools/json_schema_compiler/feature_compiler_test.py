#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import feature_compiler
import unittest

class FeatureCompilerTest(unittest.TestCase):
  """Test the FeatureCompiler. Note that we test that the expected features are
  generated more thoroughly in features_generation_unittest.cc. And, of course,
  this is most exhaustively tested through Chrome's compilation process (if a
  feature fails to parse, the compile fails).
  These tests primarily focus on catching errors during parsing.
  """
  def _parseFeature(self, value):
    """Parses a feature from the given value and returns the result."""
    f = feature_compiler.Feature('alpha')
    f.Parse(value)
    return f

  def _hasError(self, f, error):
    """Asserts that |error| is present somewhere in the given feature's
    errors."""
    self.assertTrue(f.errors)
    self.assertNotEqual(-1, str(f.errors).find(error), str(f.errors))

  def setUp(self):
    feature_compiler.ENABLE_ASSERTIONS = False
    feature_compiler.STRINGS_TO_UNICODE = True

  def testFeature(self):
    # Test some basic feature parsing for a sanity check.
    f = self._parseFeature({
      'blacklist': ['aaa', 'bbb'],
      'channel': 'stable',
      'command_line_switch': 'switch',
      'component_extensions_auto_granted': False,
      'contexts': ['blessed_extension', 'blessed_web_page'],
      'default_parent': True,
      'dependencies': ['dependency1', 'dependency2'],
      'extension_types': ['extension'],
      'location': 'component',
      'internal': True,
      'matches': ['*://*/*'],
      'max_manifest_version': 1,
      'noparent': True,
      'platforms': ['mac', 'win'],
      'whitelist': ['zzz', 'yyy']
    })
    self.assertFalse(f.errors)

  def testUnknownKeyError(self):
    f = self._parseFeature({
      'contexts': ['blessed_extension'],
      'channel': 'stable',
      'unknownkey': 'unknownvalue'
    })
    self._hasError(f, 'Unrecognized key')

  def testUnknownEnumValue(self):
    f = self._parseFeature({
      'contexts': ['blessed_extension', 'unknown_context'],
      'channel': 'stable'
    })
    self._hasError(f, 'Illegal value: "unknown_context"')

  def testImproperType(self):
    f = self._parseFeature({'min_manifest_version': '1'})
    self._hasError(f, 'Illegal value: "1"')

  def testImproperSubType(self):
    f = self._parseFeature({'dependencies': [1, 2, 3]})
    self._hasError(f, 'Illegal value: "1"')

  def testImproperValue(self):
    f = self._parseFeature({'noparent': False})
    self._hasError(f, 'Illegal value: "False"')

if __name__ == '__main__':
  unittest.main()
