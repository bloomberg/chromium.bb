# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

import browser_options

class BrowserOptionsTest(unittest.TestCase):
  def testDirectMutability(self):
    options = browser_options.BrowserOptions()
    # Should be possible to add new fields to the options object.
    options.x = 3
    self.assertEquals(3, options.x)

    # Unset fields on the options object should default to None.
    self.assertEquals(None, options.y)

  def testOptparseMutabilityWhenSpecified(self):
    options = browser_options.BrowserOptions()
    parser = options.CreateParser()
    parser.add_option("-v", dest="verbosity", action="store_true")
    options_ret, args = parser.parse_args(["-v"])
    self.assertEquals(options_ret, options)
    self.assertTrue(options.verbosity)

  def testOptparseMutabilityWhenNotSpecified(self):
    options = browser_options.BrowserOptions()

    parser = options.CreateParser()
    parser.add_option("-v", dest="verbosity", action="store_true")
    options_ret, args = parser.parse_args([])
    self.assertEquals(options_ret, options)
    self.assertFalse(options.verbosity)
