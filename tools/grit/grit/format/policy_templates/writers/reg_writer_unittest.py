#!/usr/bin/python2.4
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


'''Unit tests for grit.format.policy_templates.writers.reg_writer'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../../../..'))

import unittest

from grit.format.policy_templates.writers import writer_unittest_common


class RegWriterUnittest(writer_unittest_common.WriterUnittestCommon):
  '''Unit tests for RegWriter.'''

  NEWLINE='\r\n'

  def CompareOutputs(self, output, expected_output):
    '''Compares the output of the reg_writer with its expected output.

    Args:
      output: The output of the reg writer as returned by grit.
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
        '}', '<messages></messages>')
    output = self.GetOutput(grd, 'fr', {'_chromium': '1',}, 'reg', 'en')
    expected_output = 'Windows Registry Editor Version 5.00'
    self.CompareOutputs(output, expected_output)

  def testMainPolicy(self):
    # Tests a policy group with a single policy of type 'main'.
    grd = self.PrepareTest(
        '{'
        '  "policy_definitions": ['
        '    {'
        '      "name": "MainPolicy",'
        '      "type": "main",'
        '      "supported_on": ["chrome.win:8-"],'
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
    output = self.GetOutput(grd, 'fr', {'_google_chrome' : '1'}, 'reg', 'en')
    expected_output = self.NEWLINE.join([
        'Windows Registry Editor Version 5.00',
        '',
        '[HKEY_LOCAL_MACHINE\\Software\\Policies\\Google\\Chrome]',
        '"MainPolicy"=dword:1'])
    self.CompareOutputs(output, expected_output)

  def testStringPolicy(self):
    # Tests a policy group with a single policy of type 'string'.
    grd = self.PrepareTest(
        '{'
        '  "policy_definitions": ['
        '    {'
        '      "name": "StringPolicy",'
        '      "type": "string",'
        '      "supported_on": ["chrome.win:8-"],'
        '      "annotations": {'
        '        "example_value": "hello, world! \\\" \\\\"'
        '      }'
        '    },'
        '  ],'
        '  "placeholders": [],'
        '}',
        '<messages>'
        '  <message name="IDS_POLICY_STRINGPOLICY_CAPTION"></message>'
        '  <message name="IDS_POLICY_STRINGPOLICY_DESC"></message>'
        '</messages>')
    output = self.GetOutput(grd, 'fr', {'_chromium' : '1'}, 'reg', 'en')
    expected_output = self.NEWLINE.join([
        'Windows Registry Editor Version 5.00',
        '',
        '[HKEY_LOCAL_MACHINE\\Software\\Policies\\Chromium]',
        '"StringPolicy"="hello, world! \\\" \\\\"'])
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
        '        {"name": "ProxyServerDisabled", "value": 0},'
        '        {"name": "ProxyServerAutoDetect", "value": 1},'
        '      ],'
        '      "supported_on": ["chrome.win:8-"],'
        '      "annotations": {'
        '        "example_value": 1'
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
    output = self.GetOutput(grd, 'fr', {'_google_chrome': '1'}, 'reg', 'en')
    expected_output = self.NEWLINE.join([
        'Windows Registry Editor Version 5.00',
        '',
        '[HKEY_LOCAL_MACHINE\\Software\\Policies\\Google\\Chrome]',
        '"EnumPolicy"=dword:1'])
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
    output = self.GetOutput(grd, 'fr', {'_chromium' : '1'}, 'reg', 'en')
    expected_output = self.NEWLINE.join([
        'Windows Registry Editor Version 5.00',
        '',
        '[HKEY_LOCAL_MACHINE\\Software\\Policies\\Chromium\\ListPolicy]',
        '"1"="foo"',
        '"2"="bar"'])

  def testNonSupportedPolicy(self):
    # Tests a policy that is not supported on Windows, so it shouldn't
    # be included in the .REG file.
    grd = self.PrepareTest(
        '{'
        '  "policy_definitions": ['
        '    {'
        '      "name": "NonWindowsPolicy",'
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
        '  <message name="IDS_POLICY_NONWINDOWSPOLICY_CAPTION"></message>'
        '  <message name="IDS_POLICY_NONWINDOWSPOLICY_DESC"></message>'
        '</messages>')
    output = self.GetOutput(grd, 'fr', {'_chromium' : '1'}, 'reg', 'en')
    expected_output = self.NEWLINE.join([
        'Windows Registry Editor Version 5.00'])
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
        '        "supported_on": ["chrome.win:8-"],'
        '        "annotations": {'
        '          "example_value": ["a", "b"]'
        '        }'
        '      },{'
        '        "name": "Policy2",'
        '        "type": "string",'
        '        "supported_on": ["chrome.win:8-"],'
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
    output = self.GetOutput(grd, 'fr', {'_chromium' : '1'}, 'reg', 'en')
    expected_output = self.NEWLINE.join([
        'Windows Registry Editor Version 5.00',
        '',
        '[HKEY_LOCAL_MACHINE\\Software\\Policies\\Chromium\\Policy1]',
        '"1"="a"',
        '"2"="b"',
        '',
        '[HKEY_LOCAL_MACHINE\\Software\\Policies\\Chromium]',
        '"Policy2"="c"'])
    self.CompareOutputs(output, expected_output)


if __name__ == '__main__':
  unittest.main()
