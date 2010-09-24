# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../../..'))

import unittest

from grit.format.policy_templates import policy_template_generator
from grit.format.policy_templates.writers import mock_writer
from grit.format.policy_templates.writers import template_writer


class PolicyTemplateGeneratorUnittest(unittest.TestCase):
  '''Unit tests for policy_template_generator.py.'''

  def do_test(self, messages, policy_groups, writer):
    '''Executes a test case.

    Creates and invokes an instance of PolicyTemplateGenerator with
    the given arguments.

    Notice: Plain comments are used in test methods instead of docstrings,
    so that method names do not get overridden by the docstrings in the
    test output.

    Args:
      strings: The dictionary of localized strings.
      policy_groups: The list of policy groups as it would be loaded from
        policy_templates.json.
      writer: A writer used for this test. It is usually derived from
        mock_writer.MockWriter.
    '''
    writer.tester = self
    policy_generator = policy_template_generator.PolicyTemplateGenerator(
        messages,
        policy_groups)
    res = policy_generator.GetTemplateText(writer)
    writer.Test()
    return res

  def testSequence(self):
    # Test the sequence of invoking the basic PolicyWriter operations,
    # in case of empty input data structures.
    class LocalMockWriter(mock_writer.MockWriter):
      def __init__(self):
        self.log = 'init;'
      def Init(self):
        self.log += 'prepare;'
      def BeginTemplate(self):
        self.log += 'begin;'
      def EndTemplate(self):
        self.log += 'end;'
      def GetTemplateText(self):
        self.log += 'get_text;'
        return 'writer_result_string'
      def Test(self):
        self.tester.assertEquals(self.log,
                                 'init;prepare;begin;end;get_text;')
    result = self.do_test({}, {}, LocalMockWriter())
    self.assertEquals(result, 'writer_result_string')

  def testEmptyGroups(self):
    # Test that empty policy groups are not passed to the writer.
    messages_mock = {
      'IDS_POLICY_GROUP1_CAPTION': '',
      'IDS_POLICY_GROUP1_DESC': '',
      'IDS_POLICY_GROUP2_CAPTION': '',
      'IDS_POLICY_GROUP2_DESC': '',
      'IDS_POLICY_GROUP3_CAPTION': '',
      'IDS_POLICY_GROUP3_DESC': '',
    }
    policies_mock = [
      {'name': 'Group1', 'type': 'group', 'policies': []},
      {'name': 'Group2', 'type': 'group', 'policies': []},
      {'name': 'Group3', 'type': 'group', 'policies': []},
    ]
    class LocalMockWriter(mock_writer.MockWriter):
      def __init__(self):
        self.log = ''
      def BeginPolicyGroup(self, group):
        self.log += '['
      def EndPolicyGroup(self):
        self.log += ']'
      def Test(self):
        self.tester.assertEquals(self.log, '')
    self.do_test(messages_mock, policies_mock, LocalMockWriter())

  def testGroups(self):
    # Test that policy groups are passed to the writer in the correct order.
    messages_mock = {
      'IDS_POLICY_GROUP1_CAPTION': '',
      'IDS_POLICY_GROUP1_DESC': '',
      'IDS_POLICY_GROUP2_CAPTION': '',
      'IDS_POLICY_GROUP2_DESC': '',
      'IDS_POLICY_GROUP3_CAPTION': '',
      'IDS_POLICY_GROUP3_DESC': '',
      'IDS_POLICY_TAG1_CAPTION': '',
      'IDS_POLICY_TAG1_DESC': '',
      'IDS_POLICY_TAG2_CAPTION': '',
      'IDS_POLICY_TAG2_DESC': '',
      'IDS_POLICY_TAG3_CAPTION': '',
      'IDS_POLICY_TAG3_DESC': '',
    }
    policies_mock = [
      {'name': 'Group1', 'type': 'group', 'policies': [{'name': 'TAG1', 'type': 'mock'}]},
      {'name': 'Group2', 'type': 'group', 'policies': [{'name': 'TAG2', 'type': 'mock'}]},
      {'name': 'Group3', 'type': 'group', 'policies': [{'name': 'TAG3', 'type': 'mock'}]},
    ]
    class LocalMockWriter(mock_writer.MockWriter):
      def __init__(self):
        self.log = ''
      def BeginPolicyGroup(self, group):
        self.log += '[' + group['policies'][0]['name']
      def EndPolicyGroup(self):
        self.log += ']'
      def Test(self):
        self.tester.assertEquals(self.log, '[TAG1][TAG2][TAG3]')
    self.do_test(messages_mock, policies_mock, LocalMockWriter())

  def testGroupTexts(self):
    # Test that GUI strings are assigned correctly to policy groups.
    messages_mock = {
      'IDS_POLICY_GROUP1_CAPTION': 'string1',
      'IDS_POLICY_GROUP1_DESC': 'string2',
      'IDS_POLICY_GROUP2_CAPTION': 'string3',
      'IDS_POLICY_GROUP2_DESC': 'string4',
    }
    policy_groups_mock = [
      {'name': 'Group1', 'type': 'group', 'policies': []},
      {'name': 'Group2', 'type': 'group', 'policies': []},
    ]
    class LocalMockWriter(mock_writer.MockWriter):
      def BeginPolicyGroup(self, group):
        if group['name'] == 'Group1':
          self.tester.assertEquals(group['caption'], 'string1')
          self.tester.assertEquals(group['desc'], 'string2')
        elif group['name'] == 'Group2':
          self.tester.assertEquals(group['caption'], 'string3')
          self.tester.assertEquals(group['desc'], 'string4')
        else:
          self.tester.fail()
    self.do_test(messages_mock, policy_groups_mock, LocalMockWriter())

  def testPolicies(self):
    # Test that policies are passed to the writer in the correct order.
    messages_mock = {
      'IDS_POLICY_GROUP1_CAPTION': '',
      'IDS_POLICY_GROUP1_DESC': '',
      'IDS_POLICY_GROUP2_CAPTION': '',
      'IDS_POLICY_GROUP2_DESC': '',
      'IDS_POLICY_GROUP1POLICY1_CAPTION': '',
      'IDS_POLICY_GROUP1POLICY1_DESC': '',
      'IDS_POLICY_GROUP1POLICY2_CAPTION': '',
      'IDS_POLICY_GROUP1POLICY2_DESC': '',
      'IDS_POLICY_GROUP2POLICY3_CAPTION': '',
      'IDS_POLICY_GROUP2POLICY3_DESC': '',
    }
    policy_groups_mock = [
      {
        'name': 'Group1',
        'type': 'group',
        'policies': [
          {'name': 'Group1Policy1', 'type': 'string'},
          {'name': 'Group1Policy2', 'type': 'string'},
        ]
      },
      {
        'name': 'Group2',
        'type': 'group',
        'policies': [
          {'name': 'Group2Policy3', 'type': 'string'},
        ]
      }
    ]
    class LocalMockWriter(mock_writer.MockWriter):
      def __init__(self):
        self.policy_name = None
        self.policy_list = []
      def BeginPolicyGroup(self, group):
        self.group = group;
      def EndPolicyGroup(self):
        self.group = None
      def WritePolicy(self, policy):
        self.tester.assertEquals(policy['name'][0:6], self.group['name'])
        self.policy_list.append(policy['name'])
      def Test(self):
        self.tester.assertEquals(
          self.policy_list,
          ['Group1Policy1', 'Group1Policy2', 'Group2Policy3'])
    self.do_test(messages_mock, policy_groups_mock, LocalMockWriter())

  def testPolicyTexts(self):
    # Test that GUI strings are assigned correctly to policies.
    messages_mock = {
      'IDS_POLICY_POLICY1_CAPTION': 'string1',
      'IDS_POLICY_POLICY1_DESC': 'string2',
      'IDS_POLICY_POLICY2_CAPTION': 'string3',
      'IDS_POLICY_POLICY2_DESC': 'string4',
      'IDS_POLICY_GROUP1_CAPTION': '',
      'IDS_POLICY_GROUP1_DESC': '',
    }
    policy_groups_mock = [
      {
        'name': 'Group1',
        'type': 'group',
        'policies': [
          {'name': 'Policy1', 'type': 'string'},
          {'name': 'Policy2', 'type': 'string'}
        ]
      }
    ]
    class LocalMockWriter(mock_writer.MockWriter):
      def WritePolicy(self, policy):
        if policy['name'] == 'Policy1':
          self.tester.assertEquals(policy['caption'], 'string1')
          self.tester.assertEquals(policy['desc'], 'string2')
        elif policy['name'] == 'Policy2':
          self.tester.assertEquals(policy['caption'], 'string3')
          self.tester.assertEquals(policy['desc'], 'string4')
        else:
          self.tester.fail()
    self.do_test(messages_mock, policy_groups_mock, LocalMockWriter())

  def testEnumTexts(self):
    # Test that GUI strings are assigned correctly to enums
    # (aka dropdown menus).
    messages_mock = {
      'IDS_POLICY_ENUM_ITEM1_CAPTION': 'string1',
      'IDS_POLICY_ENUM_ITEM2_CAPTION': 'string2',
      'IDS_POLICY_ENUM_ITEM3_CAPTION': 'string3',
      'IDS_POLICY_POLICY1_CAPTION': '',
      'IDS_POLICY_POLICY1_DESC': '',

    }
    policy_groups_mock = [{
      'name': 'Policy1',
      'type': 'enum',
      'items': [
        {'name': 'item1', 'value': '0'},
        {'name': 'item2', 'value': '1'},
        {'name': 'item3', 'value': '3'},
      ]
    }]

    class LocalMockWriter(mock_writer.MockWriter):
      def WritePolicy(self, policy):
        self.tester.assertEquals(policy['items'][0]['caption'], 'string1')
        self.tester.assertEquals(policy['items'][1]['caption'], 'string2')
        self.tester.assertEquals(policy['items'][2]['caption'], 'string3')
    self.do_test(messages_mock, policy_groups_mock, LocalMockWriter())

  def testPolicyFiltering(self):
    # Test that policies are filtered correctly based on their annotations.
    messages_mock = {
      'IDS_POLICY_GROUP1_CAPTION': '',
      'IDS_POLICY_GROUP1_DESC': '',
      'IDS_POLICY_GROUP2_CAPTION': '',
      'IDS_POLICY_GROUP2_DESC': '',
      'IDS_POLICY_GROUP1POLICY1_CAPTION': '',
      'IDS_POLICY_GROUP1POLICY1_DESC': '',
      'IDS_POLICY_GROUP1POLICY2_CAPTION': '',
      'IDS_POLICY_GROUP1POLICY2_DESC': '',
      'IDS_POLICY_GROUP2POLICY3_CAPTION': '',
      'IDS_POLICY_GROUP2POLICY3_DESC': '',
    }
    policy_groups_mock = [{
      'name': 'Group1',
      'type': 'group',
      'policies': [
        {
          'name': 'Group1Policy1',
          'type': 'string',
          'annotations': {'platforms': ['aaa', 'bbb', 'ccc']}
        },
        {
          'name': 'Group1Policy2',
          'type': 'string',
          'annotations': {'platforms': ['ddd']}
        },
      ]
    },{
      'name': 'Group2',
      'type': 'group',
      'policies': [
        {
          'name': 'Group2Policy3',
          'type': 'string',
          'annotations': {'platforms': ['eee']}
        },
      ]
    }]
    class LocalMockWriter(mock_writer.MockWriter):
      def __init__(self, platforms, expected_list):
        self.platforms = platforms
        self.expected_list = expected_list
        self.policy_name = None
        self.result_list = []
      def BeginPolicyGroup(self, group):
        self.group = group;
        self.result_list.append('begin_' + group['name'])
      def EndPolicyGroup(self):
        self.group = None
      def WritePolicy(self, policy):
        self.tester.assertEquals(policy['name'][0:6], self.group['name'])
        self.result_list.append(policy['name'])
      def IsPolicySupported(self, policy):
        # Call the original (non-mock) implementation of this method.
        return template_writer.TemplateWriter.IsPolicySupported(self, policy)
      def Test(self):
        self.tester.assertEquals(
          self.result_list,
          self.expected_list)
    self.do_test(
        messages_mock,
        policy_groups_mock,
        LocalMockWriter(['eee'], ['begin_Group2', 'Group2Policy3']))
    self.do_test(
        messages_mock,
        policy_groups_mock,
        LocalMockWriter(
            ['ddd', 'bbb'],
            ['begin_Group1', 'Group1Policy1', 'Group1Policy2']))

if __name__ == '__main__':
  unittest.main()
