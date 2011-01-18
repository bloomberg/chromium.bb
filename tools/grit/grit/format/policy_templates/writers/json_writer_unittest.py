#!/usr/bin/python2.4
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
    # Test the handling of an empty policy list.
    grd = self.PrepareTest(
        '{'
        '  "policy_definitions": [],'
        '  "placeholders": [],'
        '  "messages": {},'
        '}')
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
        '      "caption": "",'
        '      "desc": "",'
        '      "supported_on": ["chrome.linux:8-"],'
        '      "example_value": True'
        '    },'
        '  ],'
        '  "placeholders": [],'
        '  "messages": {},'
        '}')
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
        '      "caption": "",'
        '      "desc": "",'
        '      "supported_on": ["chrome.linux:8-"],'
        '      "example_value": "hello, world!"'
        '    },'
        '  ],'
        '  "placeholders": [],'
        '  "messages": {},'
        '}')
    output = self.GetOutput(grd, 'fr', {'_chromium' : '1'}, 'json', 'en')
    expected_output = (
        '{\n'
        '  "StringPolicy": "hello, world!"\n'
        '}')
    self.CompareOutputs(output, expected_output)

  def testIntPolicy(self):
    # Tests a policy group with a single policy of type 'string'.
    grd = self.PrepareTest(
        '{'
        '  "policy_definitions": ['
        '    {'
        '      "name": "IntPolicy",'
        '      "type": "int",'
        '      "caption": "",'
        '      "desc": "",'
        '      "supported_on": ["chrome.linux:8-"],'
        '      "example_value": 15'
        '    },'
        '  ],'
        '  "placeholders": [],'
        '  "messages": {},'
        '}')
    output = self.GetOutput(grd, 'fr', {'_chromium' : '1'}, 'json', 'en')
    expected_output = (
        '{\n'
        '  "IntPolicy": 15\n'
        '}')
    self.CompareOutputs(output, expected_output)

  def testIntEnumPolicy(self):
    # Tests a policy group with a single policy of type 'int-enum'.
    grd = self.PrepareTest(
        '{'
        '  "policy_definitions": ['
        '    {'
        '      "name": "EnumPolicy",'
        '      "type": "int-enum",'
        '      "caption": "",'
        '      "desc": "",'
        '      "items": ['
        '        {"name": "ProxyServerDisabled", "value": 0, "caption": ""},'
        '        {"name": "ProxyServerAutoDetect", "value": 1, "caption": ""},'
        '      ],'
        '      "supported_on": ["chrome.linux:8-"],'
        '      "example_value": 1'
        '    },'
        '  ],'
        '  "placeholders": [],'
        '  "messages": {},'
        '}')
    output = self.GetOutput(grd, 'fr', {'_google_chrome': '1'}, 'json', 'en')
    expected_output = (
        '{\n'
        '  "EnumPolicy": 1\n'
        '}')
    self.CompareOutputs(output, expected_output)

  def testStringEnumPolicy(self):
    # Tests a policy group with a single policy of type 'string-enum'.
    grd = self.PrepareTest(
        '{'
        '  "policy_definitions": ['
        '    {'
        '      "name": "EnumPolicy",'
        '      "type": "string-enum",'
        '      "caption": "",'
        '      "desc": "",'
        '      "items": ['
        '        {"name": "ProxyServerDisabled", "value": "one",'
        '         "caption": ""},'
        '        {"name": "ProxyServerAutoDetect", "value": "two",'
        '         "caption": ""},'
        '      ],'
        '      "supported_on": ["chrome.linux:8-"],'
        '      "example_value": "one"'
        '    },'
        '  ],'
        '  "placeholders": [],'
        '  "messages": {},'
        '}')
    output = self.GetOutput(grd, 'fr', {'_google_chrome': '1'}, 'json', 'en')
    expected_output = (
        '{\n'
        '  "EnumPolicy": "one"\n'
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
        '      "caption": "",'
        '      "desc": "",'
        '      "supported_on": ["chrome.linux:8-"],'
        '      "example_value": ["foo", "bar"]'
        '    },'
        '  ],'
        '  "placeholders": [],'
        '  "messages": {},'
        '}')
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
        '      "caption": "",'
        '      "desc": "",'
        '      "supported_on": ["chrome.mac:8-"],'
        '      "example_value": ["a"]'
        '    },'
        '  ],'
        '  "placeholders": [],'
        '  "messages": {},'
        '}')
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
        '      "caption": "",'
        '      "desc": "",'
        '      "policies": [{'
        '        "name": "Policy1",'
        '        "type": "list",'
        '        "caption": "",'
        '        "desc": "",'
        '        "supported_on": ["chrome.linux:8-"],'
        '        "example_value": ["a", "b"]'
        '      },{'
        '        "name": "Policy2",'
        '        "type": "string",'
        '        "caption": "",'
        '        "desc": "",'
        '        "supported_on": ["chrome.linux:8-"],'
        '        "example_value": "c"'
        '      }],'
        '    },'
        '  ],'
        '  "placeholders": [],'
        '  "messages": {},'
        '}')
    output = self.GetOutput(grd, 'fr', {'_chromium' : '1'}, 'json', 'en')
    expected_output = (
        '{\n'
        '  "Policy1": ["a", "b"],\n'
        '  "Policy2": "c"\n'
        '}')
    self.CompareOutputs(output, expected_output)


if __name__ == '__main__':
  unittest.main()
