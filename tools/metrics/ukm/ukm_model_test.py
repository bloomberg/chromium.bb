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
  <owner>anotherowner@chromium.org</owner>
  <summary>
    Event1 summary.
  </summary>
  <metric name="Metric1">
    <owner>owner2@chromium.org</owner>
    <summary>
      Metric1 summary.
    </summary>
    <aggregation>
      <history>
        <index fields="profile.country"/>
        <index fields="profile.form_factor"/>
        <statistics>
          <quantiles type="std-percentiles"/>
        </statistics>
      </history>
    </aggregation>
  </metric>
  <metric name="Metric2">
    <aggregation>
      <history>
        <statistics>
          <enumeration export="False"/>
        </statistics>
      </history>
    </aggregation>
  </metric>
  <metric name="Metric3"/>
</event>

</ukm-configuration>
""".strip()

CONFIG_EVENT_NAMES_SORTED = """
<ukm-configuration>

<event name="Event1"/>

<event name="Event2"/>

<event name="Event3"/>

</ukm-configuration>
""".strip()

CONFIG_EVENT_NAMES_UNSORTED = """
<ukm-configuration>

<event name="Event2"/>

<event name="Event3"/>

<event name="Event1"/>

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

  def testSortByEventName(self):
    result = ukm_model.UpdateXML(CONFIG_EVENT_NAMES_UNSORTED)
    self.assertMultiLineEqual(CONFIG_EVENT_NAMES_SORTED, result.strip())

if __name__ == '__main__':
  unittest.main()
