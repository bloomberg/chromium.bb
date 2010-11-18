#!/usr/bin/python2.4
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.format.policy_templates.writers.json_writer'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../../../..'))

import unittest

from grit.format.policy_templates.writers import writer_unittest_common


class JsonWriterUnittest(writer_unittest_common.WriterUnittestCommon):
  '''Unit tests for JsonWriter.'''

  def CompareOutputs(self, output, expected_output):
    '''Compares the output of the json_writer with its expected output.

    Args:
      output: The output of the json writer as returned by grit.
      expected_output: The expected output.

    Raises:
      AssertionError: if the two strings are not equivalent.
    '''
    self.assertEquals(
        output.strip(),
        expected_output.strip())

  def testEmpty(self):
    # Test PListWriter in case of empty polices.
    grd = self.PrepareTest(
        '{'
        '  "policy_definitions": [],'
        '  "placeholders": [],'
        '}', '<messages></messages>')
    output = self.GetOutput(grd, 'fr', {'_chromium': '1',}, 'json', 'en')
    expected_output = '{\n}'
    self.CompareOutputs(output, expected_output)

  def testMainPolicy(self):
    # Tests a policy group with a single policy of type 'main'.
    grd = self.PrepareTest(
        '{'
        '  "policy_definitions": ['
        '    {'
        '      "name": "MainPolicy",'
        '      "type": "main",'
        '      "supported_on": ["chrome.linux:8-"],'
        '      "annotations": {'
        '        "example_value": True'
        '      }'
        '    },'
        '  ],'
        '  "placeholders": [],'
        '}',
        '<messages>'
        '  <message name="IDS_POLICY_MAINPOLICY_CAPTION"></message>'
        '  <message name="IDS_POLICY_MAINPOLICY_DESC"></message>'
        '</messages>')
    output = self.GetOutput(grd, 'fr', {'_google_chrome' : '1'}, 'json', 'en')
    expected_output = (
        '{\n'
        '  "MainPolicy": true\n'
        '}')
    self.CompareOutputs(output, expected_output)

  def testStringPolicy(self):
    # Tests a policy group with a single policy of type 'string'.
    grd = self.PrepareTest(
        '{'
        '  "policy_definitions": ['
        '    {'
        '      "name": "StringPolicy",'
        '      "type": "string",'
        '      "supported_on": ["chrome.linux:8-"],'
        '      "annotations": {'
        '        "example_value": "hello, world!"'
        '      }'
        '    },'
        '  ],'
        '  "placeholders": [],'
        '}',
        '<messages>'
        '  <message name="IDS_POLICY_STRINGPOLICY_CAPTION"></message>'
        '  <message name="IDS_POLICY_STRINGPOLICY_DESC"></message>'
        '</messages>')
    output = self.GetOutput(grd, 'fr', {'_chromium' : '1'}, 'json', 'en')
    expected_output = (
        '{\n'
        '  "StringPolicy": "hello, world!"\n'
        '}')
    self.CompareOutputs(output, expected_output)

  def testEnumPolicy(self):
    # Tests a policy group with a single policy of type 'enum'.
    grd = self.PrepareTest(
        '{'
        '  "policy_definitions": ['
        '    {'
        '      "name": "EnumPolicy",'
        '      "type": "enum",'
        '      "items": ['
        '        {"name": "ProxyServerDisabled", "value": "0"},'
        '        {"name": "ProxyServerAutoDetect", "value": "1"},'
        '      ],'
        '      "supported_on": ["chrome.linux:8-"],'
        '      "annotations": {'
        '        "example_value": "1"'
        '      }'
        '    },'
        '  ],'
        '  "placeholders": [],'
        '}',
        '<messages>'
        '  <message name="IDS_POLICY_ENUMPOLICY_CAPTION"></message>'
        '  <message name="IDS_POLICY_ENUMPOLICY_DESC"></message>'
        '  <message name="IDS_POLICY_ENUM_PROXYSERVERDISABLED_CAPTION">'
        '  </message>'
        '  <message name="IDS_POLICY_ENUM_PROXYSERVERAUTODETECT_CAPTION">'
        '  </message>'
        '</messages>')
    output = self.GetOutput(grd, 'fr', {'_google_chrome': '1'}, 'json', 'en')
    expected_output = (
        '{\n'
        '  "EnumPolicy": 1\n'
        '}')
    self.CompareOutputs(output, expected_output)

  def testListPolicy(self):
    # Tests a policy group with a single policy of type 'list'.
    grd = self.PrepareTest(
        '{'
        '  "policy_definitions": ['
        '    {'
        '      "name": "ListPolicy",'
        '      "type": "list",'
        '      "supported_on": ["chrome.linux:8-"],'
        '      "annotations": {'
        '        "example_value": ["foo", "bar"]'
        '      }'
        '    },'
        '  ],'
        '  "placeholders": [],'
        '}',
        '<messages>'
        '  <message name="IDS_POLICY_LISTPOLICY_DESC"></message>'
        '  <message name="IDS_POLICY_LISTPOLICY_CAPTION"></message>'
        '  <message name="IDS_POLICY_LISTPOLICY_LABEL"></message>'
        '</messages>')
    output = self.GetOutput(grd, 'fr', {'_chromium' : '1'}, 'json', 'en')
    expected_output = (
        '{\n'
        '  "ListPolicy": ["foo", "bar"]\n'
        '}')
    self.CompareOutputs(output, expected_output)

  def testNonSupportedPolicy(self):
    # Tests a policy that is not supported on Linux, so it shouldn't
    # be included in the JSON file.
    grd = self.PrepareTest(
        '{'
        '  "policy_definitions": ['
        '    {'
        '      "name": "NonLinuxPolicy",'
        '      "type": "list",'
        '      "supported_on": ["chrome.mac:8-"],'
        '      "annotations": {'
        '        "example_value": ["a"]'
        '      }'
        '    },'
        '  ],'
        '  "placeholders": [],'
        '}',
        '<messages>'
        '  <message name="IDS_POLICY_NONLINUXPOLICY_CAPTION"></message>'
        '  <message name="IDS_POLICY_NONLINUXPOLICY_DESC"></message>'
        '</messages>')
    output = self.GetOutput(grd, 'fr', {'_chromium' : '1'}, 'json', 'en')
    expected_output = '{\n}'
    self.CompareOutputs(output, expected_output)

  def testPolicyGroup(self):
    # Tests a policy group that has more than one policies.
    grd = self.PrepareTest(
        '{'
        '  "policy_definitions": ['
        '    {'
        '      "name": "Group1",'
        '      "type": "group",'
        '      "policies": [{'
        '        "name": "Policy1",'
        '        "type": "list",'
        '        "supported_on": ["chrome.linux:8-"],'
        '        "annotations": {'
        '          "example_value": ["a", "b"]'
        '        }'
        '      },{'
        '        "name": "Policy2",'
        '        "type": "string",'
        '        "supported_on": ["chrome.linux:8-"],'
        '        "annotations": {'
        '          "example_value": "c"'
        '        }'
        '      }],'
        '    },'
        '  ],'
        '  "placeholders": [],'
        '}',
        '<messages>'
        '  <message name="IDS_POLICY_GROUP1_CAPTION"></message>'
        '  <message name="IDS_POLICY_GROUP1_DESC"></message>'
        '  <message name="IDS_POLICY_POLICY1_DESC"></message>'
        '  <message name="IDS_POLICY_POLICY2_DESC"></message>'
        '  <message name="IDS_POLICY_POLICY1_CAPTION"></message>'
        '  <message name="IDS_POLICY_POLICY2_CAPTION"></message>'
        '</messages>')
    output = self.GetOutput(grd, 'fr', {'_chromium' : '1'}, 'json', 'en')
    expected_output = (
        '{\n'
        '  "Policy1": ["a", "b"],\n'
        '  "Policy2": "c"\n'
        '}')
    self.CompareOutputs(output, expected_output)


if __name__ == '__main__':
  unittest.main()
