# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import unittest
import xml.dom.minidom

import extract_histograms


class ExtractHistogramsTest(unittest.TestCase):

  def testNewHistogramWithoutSummary(self):
    histogram_without_summary = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>
 <histogram name="Test.Histogram" units="things">
   <owner>person@chromium.org</owner>
 </histogram>
</histograms>
</histogram-configuration>
""")
    (_, have_errors) = extract_histograms._ExtractHistogramsFromXmlTree(
        histogram_without_summary, {})
    self.assertTrue(have_errors)

  def testNewHistogramWithoutOwner(self):
    histogram_without_owner = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>
 <histogram name="Test.Histogram" units="things">
  <summary> This is a summary </summary>
 </histogram>
</histograms>
</histogram-configuration>
""")
    (_, have_errors) = extract_histograms._ExtractHistogramsFromXmlTree(
        histogram_without_owner, {})
    self.assertTrue(have_errors)

  def testNewHistogramWithOwnerPlaceHolder(self):
    histogram_with_owner_placeholder = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>
 <histogram name="Test.Histogram" units="things">
  <owner> Please list the metric's owners. Add more owner tags as needed.
  </owner>
  <summary> This is a summary </summary>
 </histogram>
</histograms>
</histogram-configuration>
""")
    (_, have_errors) = extract_histograms._ExtractHistogramsFromXmlTree(
        histogram_with_owner_placeholder, {})
    self.assertFalse(have_errors)


if __name__ == "__main__":
  logging.basicConfig(level=logging.ERROR + 1)
  unittest.main()
