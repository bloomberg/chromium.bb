#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simulate a passing google-test executable.

http://code.google.com/p/googletest/

The data was generated with:

1. Get the list of a few interesting test cases.
python tools/swarm_client/list_test_cases.py --pre \
  out/Release/browser_tests | grep 'ClearSiteDataOnExit\|ShutdownStartupCycle'

2. Generate XML files.
# PolicyTest.ClearSiteDataOnExit had a PRE_PRE_ test case.
testing/xvfb.py out/Release out/Release/browser_tests \
  --gtest_output=xml:ClearSiteDataOnExit.xml \
  --gtest_filter=PolicyTest.ClearSiteDataOnExit

# PredictorBrowserTest.ShutdownStartupCycle had a PRE_ test case.
testing/xvfb.py out/Release out/Release/browser_tests \
  --gtest_output=xml:ShutdownStartupCycle.xml \
  --gtest_filter=PredictorBrowserTest.ShutdownStartupCycle

# PolicyTest.Javascript is a normal test case
testing/xvfb.py out/Release out/Release/browser_tests \
  --gtest_output=xml:Javascript.xml \
  --gtest_filter=PolicyTest.Javascript

# Generate the expectation.
testing/xvfb.py out/Release out/Release/browser_tests \
  --gtest_output=xml:expected.xml \
  --gtest_filter=PolicyTest.ClearSiteDataOnExit:\
PolicyTest.Javascript:PredictorBrowserTest.ShutdownStartupCycle

3. Reduce the XML file so they are not 300kb each
python gtest_fake_xml.py --trim-xml

4. Reorder expected.xml as necessary.
"""

import glob
import optparse
import os
import shutil
import sys
from xml.dom import minidom

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))

import gtest_fake_base


TESTS = {
  'PolicyTest': [
    'Javascript',
    'ClearSiteDataOnExit',
    'PRE_ClearSiteDataOnExit',
    'PRE_PRE_ClearSiteDataOnExit',
  ],
  'PredictorBrowserTest': [
    'PRE_ShutdownStartupCycle',
    'ShutdownStartupCycle',
  ],
}
TOTAL = sum(len(v) for v in TESTS.itervalues())


def trim_xml_whitespace(data):
  """Recursively remove non-important elements."""
  children_to_remove = []
  for child in data.childNodes:
    if child.nodeType == minidom.Node.TEXT_NODE and not child.data.strip():
      children_to_remove.append(child)
    elif child.nodeType == minidom.Node.COMMENT_NODE:
      children_to_remove.append(child)
    elif child.nodeType == minidom.Node.ELEMENT_NODE:
      trim_xml_whitespace(child)
  for child in children_to_remove:
    data.removeChild(child)

def trim_xml():
  for i in glob.glob('*.xml'):
    with open(i) as f:
      data = minidom.parse(f)
    trim_xml_whitespace(data)
    root_to_delete = []
    for test_suites in data.childNodes[0].childNodes:
      if (test_suites.nodeType != minidom.Node.ELEMENT_NODE or
          test_suites.attributes.getNamedItem('name').value not in TESTS):
        root_to_delete.append(test_suites)
        continue

      for test_suite in test_suites.childNodes:
        time = test_suite.attributes.getNamedItem('time')
        if time and time.value and float(time.value):
          # Reset them all to 1.1s for comparison.
          time.value = '1.1'

    for item in root_to_delete:
      data.childNodes[0].removeChild(item)
    with open(i, 'w') as f:
      f.write(data.toprettyxml(indent='  '))


def main():
  parser = optparse.OptionParser()
  parser.add_option('--gtest_list_tests', action='store_true')
  parser.add_option('--gtest_filter')
  parser.add_option('--gtest_output')
  # Ignored, exists to fake browser_tests.
  parser.add_option('--gtest_print_time')
  # Ignored, exists to fake browser_tests.
  parser.add_option('--lib')
  parser.add_option('--trim-xml', action='store_true')
  options, args = parser.parse_args()
  if args:
    parser.error('Failed to process args %s' % args)

  if options.trim_xml:
    trim_xml()
    return 0

  if options.gtest_list_tests:
    for fixture, cases in TESTS.iteritems():
      print '%s.' % fixture
      for case in cases:
        print '  ' + case
    # Print junk.
    print '  YOU HAVE 2 tests with ignored failures (FAILS prefix)'
    print ''
    return 0

  if options.gtest_filter:
    # Simulate running one test.
    print 'Note: Google Test filter = %s\n' % options.gtest_filter
    assert 'PRE_' not in options.gtest_filter
    # To mick better the actual output, it should output the PRE_ tests but it's
    # not important for this test.
    print gtest_fake_base.get_test_output(options.gtest_filter)
    print gtest_fake_base.get_footer(1, 1)
    case = options.gtest_filter.split('.', 1)[1]
    filename = case + '.xml'
    out = os.path.join(options.gtest_output, filename)
    if case != 'Javascript':
      shutil.copyfile(os.path.join(ROOT_DIR, filename), out)
      return

    # Fails on first run, succeeds on the second.
    filename = case + '_fail.xml'
    out = os.path.join(options.gtest_output, filename)
    if not os.path.exists(out):
      shutil.copyfile(os.path.join(ROOT_DIR, filename), out)
      return 1
    else:
      filename = case + '.xml'
      out = os.path.join(options.gtest_output, filename)
      shutil.copyfile(os.path.join(ROOT_DIR, filename), out)
      return 0
  return 2


if __name__ == '__main__':
  sys.exit(main())
