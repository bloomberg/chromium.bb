#!/usr/bin/python2.4
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.format.policy_templates.writers.adm_writer'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../../../..'))

import tempfile
import unittest
import StringIO

from grit.format.policy_templates.writers import writer_unittest_common
from grit import grd_reader
from grit import util
from grit.tool import build


class AdmWriterUnittest(writer_unittest_common.WriterUnittestCommon):
  '''Unit tests for AdmWriter.'''

  def CompareOutputs(self, output, expected_output):
    '''Compares the output of the adm_writer with its expected output.

    Args:
      output: The output of the adm writer as returned by grit.
      expected_output: The expected output.

    Raises:
      AssertionError: if the two strings are not equivalent.
    '''
    self.assertEquals(
        output.strip(),
        expected_output.strip().replace('\n', '\r\n'))

  def testEmpty(self):
    # Test PListWriter in case of empty polices.
    grd = self.PrepareTest('''
      {
        'policy_groups': [],
        'placeholders': [],
      }''', '''
        <messages>
          <message name="IDS_POLICY_WIN_SUPPORTED_WINXPSP2">At least "Windows 3.11</message>
        </messages>
      ''' )
    output = self.GetOutput(grd, 'fr', {'_chromium': '1',}, 'adm', 'en')
    expected_output = '''CLASS MACHINE
  CATEGORY !!chromium
    KEYNAME "Software\\Policies\\Chromium"

  END CATEGORY

[Strings]
SUPPORTED_WINXPSP2="At least "Windows 3.11"
chromium="Chromium"'''
    self.CompareOutputs(output, expected_output)

  def testMainPolicy(self):
    # Tests a policy group with a single policy of type 'main'.
    grd = self.PrepareTest('''
      {
        'policy_groups': [
          {
            'name': 'MainGroup',
            'policies': [{
              'name': 'MainPolicy',
              'type': 'main',
              'annotations': {'platforms': ['win']}
            }],
          },
        ],
        'placeholders': [],
      }''', '''
        <messages>
          <message name="IDS_POLICY_GROUP_MAINGROUP_CAPTION">Caption of main.</message>
          <message name="IDS_POLICY_GROUP_MAINGROUP_DESC">Description of main.</message>
          <message name="IDS_POLICY_WIN_SUPPORTED_WINXPSP2">At least Windows 3.12</message>
        </messages>
      ''' )
    output = self.GetOutput(grd, 'fr', {'_google_chrome' : '1'}, 'adm', 'en')
    expected_output = '''CLASS MACHINE
  CATEGORY !!google
    CATEGORY !!googlechrome
      KEYNAME "Software\\Policies\\Google\\Chrome"

      POLICY !!MainGroup_Policy
        #if version >= 4
          SUPPORTED !!SUPPORTED_WINXPSP2
        #endif
        EXPLAIN !!MainGroup_Explain
        VALUENAME "MainPolicy"
        VALUEON NUMERIC 1
        VALUEOFF NUMERIC 0
      END POLICY

    END CATEGORY
  END CATEGORY

[Strings]
SUPPORTED_WINXPSP2="At least Windows 3.12"
google="Google"
googlechrome="Google Chrome"
MainGroup_Policy="Caption of main."
MainGroup_Explain="Description of main."'''
    self.CompareOutputs(output, expected_output)

  def testStringPolicy(self):
    # Tests a policy group with a single policy of type 'string'.
    grd = self.PrepareTest('''
      {
        'policy_groups': [
          {
            'name': 'StringGroup',
            'policies': [{
              'name': 'StringPolicy',
              'type': 'string',
              'annotations': {'platforms': ['win']}
            }],
          },
        ],
        'placeholders': [],
      }''', '''
        <messages>
          <message name="IDS_POLICY_GROUP_STRINGGROUP_CAPTION">Caption of group.</message>
          <message name="IDS_POLICY_GROUP_STRINGGROUP_DESC">Description of group.
With a newline.</message>
          <message name="IDS_POLICY_STRINGPOLICY_CAPTION">Caption of policy.</message>
          <message name="IDS_POLICY_WIN_SUPPORTED_WINXPSP2">At least Windows 3.13</message>
        </messages>
      ''' )
    output = self.GetOutput(grd, 'fr', {'_chromium' : '1'}, 'adm', 'en')
    expected_output = '''CLASS MACHINE
  CATEGORY !!chromium
    KEYNAME "Software\\Policies\\Chromium"

    POLICY !!StringGroup_Policy
      #if version >= 4
        SUPPORTED !!SUPPORTED_WINXPSP2
      #endif
      EXPLAIN !!StringGroup_Explain

      PART !!StringPolicy_Part  EDITTEXT
        VALUENAME "StringPolicy"
      END PART
    END POLICY

  END CATEGORY

[Strings]
SUPPORTED_WINXPSP2="At least Windows 3.13"
chromium="Chromium"
StringGroup_Policy="Caption of group."
StringGroup_Explain="Description of group.\\nWith a newline."
StringPolicy_Part="Caption of policy."
'''
    self.CompareOutputs(output, expected_output)

  def testEnumPolicy(self):
    # Tests a policy group with a single policy of type 'enum'.
    grd = self.PrepareTest('''
      {
        'policy_groups': [
          {
            'name': 'EnumGroup',
            'policies': [{
              'name': 'EnumPolicy',
              'type': 'enum',
              'items': [
                {'name': 'ProxyServerDisabled', 'value': '0'},
                {'name': 'ProxyServerAutoDetect', 'value': '1'},
              ],
              'annotations': {'platforms': ['win']}
            }],
          },
        ],
        'placeholders': [],
      }''', '''
        <messages>
          <message name="IDS_POLICY_GROUP_ENUMGROUP_CAPTION">Caption of group.</message>
          <message name="IDS_POLICY_GROUP_ENUMGROUP_DESC">Description of group.</message>
          <message name="IDS_POLICY_ENUMPOLICY_CAPTION">Caption of policy.</message>
          <message name="IDS_POLICY_ENUMPOLICY_DESC">Description of policy.</message>
          <message name="IDS_POLICY_ENUM_PROXYSERVERDISABLED_CAPTION">Option1</message>
          <message name="IDS_POLICY_ENUM_PROXYSERVERAUTODETECT_CAPTION">Option2</message>
          <message name="IDS_POLICY_WIN_SUPPORTED_WINXPSP2">At least Windows 3.14</message>
        </messages>
      ''' )
    output = self.GetOutput(grd, 'fr', {'_google_chrome': '1'}, 'adm', 'en')
    expected_output = '''CLASS MACHINE
  CATEGORY !!google
    CATEGORY !!googlechrome
      KEYNAME "Software\\Policies\\Google\\Chrome"

      POLICY !!EnumGroup_Policy
        #if version >= 4
          SUPPORTED !!SUPPORTED_WINXPSP2
        #endif
        EXPLAIN !!EnumGroup_Explain

        PART !!EnumPolicy_Part  DROPDOWNLIST
          VALUENAME "EnumPolicy"
          ITEMLIST
            NAME !!ProxyServerDisabled_DropDown VALUE NUMERIC 0
            NAME !!ProxyServerAutoDetect_DropDown VALUE NUMERIC 1
          END ITEMLIST
        END PART
      END POLICY

    END CATEGORY
  END CATEGORY

[Strings]
SUPPORTED_WINXPSP2="At least Windows 3.14"
google="Google"
googlechrome="Google Chrome"
EnumGroup_Policy="Caption of group."
EnumGroup_Explain="Description of group."
EnumPolicy_Part="Caption of policy."
ProxyServerDisabled_DropDown="Option1"
ProxyServerAutoDetect_DropDown="Option2"
'''
    self.CompareOutputs(output, expected_output)

  def testListPolicy(self):
    # Tests a policy group with a single policy of type 'list'.
    grd = self.PrepareTest('''
      {
        'policy_groups': [
          {
            'name': 'ListGroup',
            'policies': [{
              'name': 'ListPolicy',
              'type': 'list',
              'annotations': {'platforms': ['win']}
            }],
          },
        ],
        'placeholders': [],
      }''', '''
        <messages>
          <message name="IDS_POLICY_GROUP_LISTGROUP_CAPTION">Caption of list group.</message>
          <message name="IDS_POLICY_GROUP_LISTGROUP_DESC">Description of list group.
With a newline.</message>
          <message name="IDS_POLICY_LISTPOLICY_CAPTION">Caption of list policy.</message>
          <message name="IDS_POLICY_WIN_SUPPORTED_WINXPSP2">At least Windows 3.15</message>
        </messages>
      ''')
    output = self.GetOutput(grd, 'fr', {'_chromium' : '1'}, 'adm', 'en')
    expected_output = '''CLASS MACHINE
  CATEGORY !!chromium
    KEYNAME "Software\\Policies\\Chromium"

    POLICY !!ListGroup_Policy
      #if version >= 4
        SUPPORTED !!SUPPORTED_WINXPSP2
      #endif
      EXPLAIN !!ListGroup_Explain

      PART !!ListPolicy_Part  LISTBOX
        KEYNAME "Software\\Policies\\Chromium\\ListPolicy"
        VALUEPREFIX ""
      END PART
    END POLICY

  END CATEGORY

[Strings]
SUPPORTED_WINXPSP2="At least Windows 3.15"
chromium="Chromium"
ListGroup_Policy="Caption of list group."
ListGroup_Explain="Description of list group.\\nWith a newline."
ListPolicy_Part="Caption of list policy."
'''
    self.CompareOutputs(output, expected_output)

  def testNonSupportedPolicy(self):
    # Tests a policy that is not supported on Windows, so it shouldn't
    # be included in the ADM file.
    grd = self.PrepareTest('''
      {
        'policy_groups': [
          {
            'name': 'NonWinGroup',
            'policies': [{
              'name': 'NonWinPolicy',
              'type': 'list',
              'annotations': {'platforms': ['linux', 'mac']}
            }],
          },
        ],
        'placeholders': [],
      }''', '''
        <messages>
          <message name="IDS_POLICY_GROUP_NONWINGROUP_CAPTION">Group caption.</message>
          <message name="IDS_POLICY_GROUP_NONWINGROUP_DESC">Group description.</message>
          <message name="IDS_POLICY_NONWINPOLICY_CAPTION">Caption of list policy.</message>
          <message name="IDS_POLICY_WIN_SUPPORTED_WINXPSP2">At least Windows 3.16</message>
        </messages>
      ''')
    output = self.GetOutput(grd, 'fr', {'_chromium' : '1'}, 'adm', 'en')
    expected_output = '''CLASS MACHINE
  CATEGORY !!chromium
    KEYNAME "Software\\Policies\\Chromium"

  END CATEGORY

[Strings]
SUPPORTED_WINXPSP2="At least Windows 3.16"
chromium="Chromium"
'''
    self.CompareOutputs(output, expected_output)

if __name__ == '__main__':
  unittest.main()
