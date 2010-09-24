#!/usr/bin/python2.4
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.format.policy_templates.writers.plist_strings_writer'''


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


class PListStringsWriterUnittest(writer_unittest_common.WriterUnittestCommon):
  '''Unit tests for PListStringsWriter.'''

  def testEmpty(self):
    # Test PListStringsWriter in case of empty polices.
    grd = self.PrepareTest('''
      {
        'policy_definitions': [],
        'placeholders': [],
      }''', '''
        <messages>
          <message name="IDS_POLICY_MAC_CHROME_PREFERENCES">$1 preferen"ces</message>
        </messages>
      ''' )
    output = self.GetOutput(
        grd,
        'fr',
        {'_chromium': '1', 'mac_bundle_id': 'com.example.Test'},
        'plist_strings',
        'en')
    expected_output = \
'''Chromium.pfm_title = "Chromium";
Chromium.pfm_description = "Chromium preferen\\"ces";
'''
    self.assertEquals(output.strip(), expected_output.strip())

  def testMainPolicy(self):
    # Tests a policy group with a single policy of type 'main'.
    grd = self.PrepareTest('''
      {
        'policy_definitions': [
          {
            'name': 'MainGroup',
            'type': 'group',
            'policies': [{
              'name': 'MainPolicy',
              'type': 'main',
              'annotations': {'platforms': ['mac']},
            }],
          },
        ],
        'placeholders': [],
      }''', '''
        <messages>
          <message name="IDS_POLICY_MAINGROUP_CAPTION">Caption of main.</message>
          <message name="IDS_POLICY_MAINGROUP_DESC">Title of main.</message>
          <message name="IDS_POLICY_MAINPOLICY_CAPTION">Caption of main policy.</message>
          <message name="IDS_POLICY_MAINPOLICY_DESC">Title of main policy.</message>
          <message name="IDS_POLICY_MAC_CHROME_PREFERENCES">Preferences of $1</message>
        </messages>
      ''' )
    output = self.GetOutput(
        grd,
        'fr',
        {'_google_chrome' : '1', 'mac_bundle_id': 'com.example.Test'},
        'plist_strings',
        'en')
    expected_output = \
'''Google Chrome.pfm_title = "Google Chrome";
Google Chrome.pfm_description = "Preferences of Google Chrome";
MainPolicy.pfm_title = "Caption of main policy.";
MainPolicy.pfm_description = "Title of main policy.";
'''
    self.assertEquals(output.strip(), expected_output.strip())

  def testStringPolicy(self):
    # Tests a policy group with a single policy of type 'string'. Also test
    # inheriting group description to policy description.
    grd = self.PrepareTest('''
      {
        'policy_definitions': [
          {
            'name': 'StringGroup',
            'type': 'group',
            'policies': [{
              'name': 'StringPolicy',
              'type': 'string',
              'annotations': {'platforms': ['mac']},
            }],
          },
        ],
        'placeholders': [],
      }''', '''
        <messages>
          <message name="IDS_POLICY_STRINGGROUP_CAPTION">Caption of group.</message>
          <message name="IDS_POLICY_STRINGGROUP_DESC">Description of group.
With a newline.</message>
          <message name="IDS_POLICY_STRINGPOLICY_CAPTION">Caption of policy.</message>
          <message name="IDS_POLICY_STRINGPOLICY_DESC">Description of policy.
With a newline.</message>
          <message name="IDS_POLICY_MAC_CHROME_PREFERENCES">Preferences Of $1</message>
        </messages>
      ''' )
    output = self.GetOutput(
        grd,
        'fr',
        {'_chromium' : '1', 'mac_bundle_id': 'com.example.Test'},
        'plist_strings',
        'en')
    expected_output = \
'''Chromium.pfm_title = "Chromium";
Chromium.pfm_description = "Preferences Of Chromium";
StringPolicy.pfm_title = "Caption of policy.";
StringPolicy.pfm_description = "Description of policy.\\nWith a newline.";
        '''
    self.assertEquals(output.strip(), expected_output.strip())

  def testEnumPolicy(self):
    # Tests a policy group with a single policy of type 'enum'.
    grd = self.PrepareTest('''
      {
        'policy_definitions': [
          {
            'name': 'EnumGroup',
            'type': 'group',
            'policies': [{
              'name': 'EnumPolicy',
              'type': 'enum',
              'items': [
                {'name': 'ProxyServerDisabled', 'value': '0'},
                {'name': 'ProxyServerAutoDetect', 'value': '1'},
              ],
              'annotations': {'platforms': ['mac']},
            }],
          },
        ],
        'placeholders': [],
      }''', '''
        <messages>
          <message name="IDS_POLICY_ENUMGROUP_CAPTION">Caption of group.</message>
          <message name="IDS_POLICY_ENUMGROUP_DESC">Description of group.</message>
          <message name="IDS_POLICY_ENUMPOLICY_CAPTION">Caption of policy.</message>
          <message name="IDS_POLICY_ENUMPOLICY_DESC">Description of policy.</message>
          <message name="IDS_POLICY_ENUM_PROXYSERVERDISABLED_CAPTION">Option1</message>
          <message name="IDS_POLICY_ENUM_PROXYSERVERAUTODETECT_CAPTION">Option2</message>
          <message name="IDS_POLICY_MAC_CHROME_PREFERENCES">$1 preferences</message>
        </messages>
      ''' )
    output = self.GetOutput(
        grd,
        'fr',
        {'_google_chrome': '1', 'mac_bundle_id': 'com.example.Test2'},
        'plist_strings',
        'en')
    expected_output = \
'''Google Chrome.pfm_title = "Google Chrome";
Google Chrome.pfm_description = "Google Chrome preferences";
EnumPolicy.pfm_title = "Caption of policy.";
EnumPolicy.pfm_description = "0 - Option1\\n1 - Option2\\nDescription of policy.";
        '''
    self.assertEquals(output.strip(), expected_output.strip())

  def testNonSupportedPolicy(self):
    # Tests a policy that is not supported on Mac, so its strings shouldn't
    # be included in the plist string table.
    grd = self.PrepareTest('''
      {
        'policy_definitions': [
          {
            'name': 'NonMacGroup',
            'type': 'group',
            'policies': [{
              'name': 'NonMacPolicy',
              'type': 'string',
              'annotations': {'platforms': ['win', 'linux']},
            }],
          },
        ],
        'placeholders': [],
      }''', '''
        <messages>
          <message name="IDS_POLICY_NONMACGROUP_CAPTION">Caption of group.</message>
          <message name="IDS_POLICY_NONMACGROUP_DESC">Description of group.</message>
          <message name="IDS_POLICY_NONMACPOLICY_CAPTION">Caption of policy.</message>
          <message name="IDS_POLICY_NONMACPOLICY_DESC">Description of policy.</message>
          <message name="IDS_POLICY_MAC_CHROME_PREFERENCES">$1 preferences</message>
        </messages>
      ''' )
    output = self.GetOutput(
        grd,
        'fr',
        {'_google_chrome': '1', 'mac_bundle_id': 'com.example.Test2'},
        'plist_strings',
        'en')
    expected_output = \
'''Google Chrome.pfm_title = "Google Chrome";
Google Chrome.pfm_description = "Google Chrome preferences";
        '''
    self.assertEquals(output.strip(), expected_output.strip())


if __name__ == '__main__':
  unittest.main()
