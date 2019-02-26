#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for writers.google_adml_writer."""

import unittest
from writers import google_adml_writer


class GoogleAdmlWriterUnittest(unittest.TestCase):

  def setUp(self):
    self.writer = google_adml_writer.GetWriter(None)  # Config unused

  def testGoogleAdml(self):
    output = self.writer.WriteTemplate(None)  # Template unused

    # No point to duplicate the full XML.
    self.assertTrue('<string id="google">Google</string>' in output)


if __name__ == '__main__':
  unittest.main()
