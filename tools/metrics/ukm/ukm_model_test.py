#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import ukm_model


PRETTY_XML = """
<!-- Comment1 -->

<ukm-configuration>

<event name="Event1">
  <owner>owner@chromium.org</owner>
  <summary>
    Event1 summary.
  </summary>
  <metric name="Metric1">
    <owner>owner2@chromium.org</owner>
    <summary>
      Metric1 summary.
    </summary>
  </metric>
  <metric name="Metric2"/>
</event>

</ukm-configuration>
""".strip()


class UkmXmlTest(unittest.TestCase):

  def testIsPretty(self):
    result = ukm_model.UpdateXML(PRETTY_XML)
    self.assertMultiLineEqual(PRETTY_XML, result.strip())

  def testHasBadEventName(self):
    bad_xml = PRETTY_XML.replace('Event1', 'Event:1')
    with self.assertRaises(ValueError) as context:
      ukm_model.UpdateXML(bad_xml)
    self.assertIn('Event:1', str(context.exception))
    self.assertIn('does not match regex', str(context.exception))

  def testHasBadMetricName(self):
    bad_xml = PRETTY_XML.replace('Metric1', 'Metric:1')
    with self.assertRaises(ValueError) as context:
      ukm_model.UpdateXML(bad_xml)
    self.assertIn('Metric:1', str(context.exception))
    self.assertIn('does not match regex', str(context.exception))


if __name__ == '__main__':
  unittest.main()
