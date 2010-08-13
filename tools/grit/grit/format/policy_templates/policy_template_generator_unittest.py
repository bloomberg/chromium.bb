# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import unittest

from policy_template_generator import PolicyTemplateGenerator
from writers.mock_writer import MockWriter


class PolicyTemplateGeneratorUnittest(unittest.TestCase):
  '''Unit tests for policy_template_generator.py.'''

  def do_test(self, strings, policy_groups, writer):
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
    policy_generator = PolicyTemplateGenerator(strings, policy_groups)
    res = policy_generator.GetTemplateText(writer)
    writer.Test()
    return res

  def testSequence(self):
    # Test the sequence of invoking the basic PolicyWriter operations,
    # in case of empty input data structures.
    class LocalMockWriter(MockWriter):
      def __init__(self):
        self.log = 'init;'
      def Prepare(self):
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
    # Test that in case of three empty policy groups, each one is passed to
    # the writer.
    strings_mock = {
      'IDS_POLICY_GROUP_GROUP1_CAPTION': None,
      'IDS_POLICY_GROUP_GROUP1_DESC': None,
      'IDS_POLICY_GROUP_GROUP2_CAPTION': None,
      'IDS_POLICY_GROUP_GROUP2_DESC': None,
      'IDS_POLICY_GROUP_GROUP3_CAPTION': None,
      'IDS_POLICY_GROUP_GROUP3_DESC': None,
    }
    policies_mock = {
      'Group1': {'tag': 'T1'},
      'Group2': {'tag': 'T2'},
      'Group3': {'tag': 'T3'},
    }
    class LocalMockWriter(MockWriter):
      def __init__(self):
        self.log = ''
        self.set = set()
      def BeginPolicyGroup(self, group_name, group):
        self.log += '['
        self.set.add ( (group_name, group['tag']) )
      def EndPolicyGroup(self):
        self.log += ']'
      def Test(self):
        self.tester.assertEquals(self.log, '[][][]')
        self.tester.assertEquals(
          self.set,
          set([('Group1', 'T1'), ('Group2', 'T2'), ('Group3', 'T3')]))
    self.do_test(strings_mock, policies_mock, LocalMockWriter())

  def testGroupTexts(self):
    # Test that GUI strings are assigned correctly to policy groups.
    strings_mock = {
      'IDS_POLICY_GROUP_GROUP1_CAPTION': 'string1',
      'IDS_POLICY_GROUP_GROUP1_DESC': 'string2',
      'IDS_POLICY_GROUP_GROUP2_CAPTION': 'string3',
      'IDS_POLICY_GROUP_GROUP2_DESC': 'string4',
    }
    policies_mock = {
      'Group1': {},
      'Group2': {},
    }
    class LocalMockWriter(MockWriter):
      def BeginPolicyGroup(self, group_name, group):
        if group_name == 'Group1':
          self.tester.assertEquals(group['caption'], 'string1')
          self.tester.assertEquals(group['desc'], 'string2')
        elif group_name == 'Group2':
          self.tester.assertEquals(group['caption'], 'string3')
          self.tester.assertEquals(group['desc'], 'string4')
        else:
          self.tester.fail()
    self.do_test(strings_mock, policies_mock, LocalMockWriter())

  def testPolicies(self):
    # Test that policies are passed correctly to the writer.
    strings_mock = {
      'IDS_POLICY_GROUP_GROUP1_CAPTION': None,
      'IDS_POLICY_GROUP_GROUP1_DESC': None,
      'IDS_POLICY_GROUP_GROUP2_CAPTION': None,
      'IDS_POLICY_GROUP_GROUP2_DESC': None,
      'IDS_POLICY_GROUP1POLICY1_CAPTION': None,
      'IDS_POLICY_GROUP1POLICY2_CAPTION': None,
      'IDS_POLICY_GROUP2POLICY3_CAPTION': None,
    }
    policies_mock = {
      'Group1': {
        'policies': {
          'Group1Policy1': {'type': 'string'},
          'Group1Policy2': {'type': 'string'}
        }
      },
      'Group2': {
        'policies': {
          'Group2Policy3': {'type': 'string'}
        }
      }
    }
    class LocalMockWriter(MockWriter):
      def __init__(self):
        self.policy_name = None
        self.policy_set = set()
      def BeginPolicyGroup(self, group_name, group):
        self.group_name = group_name
      def EndPolicyGroup(self):
        self.group_name = None
      def WritePolicy(self, group_name, group):
        self.tester.assertEquals(group_name[0:6], self.group_name)
        self.policy_set.add(group_name)
      def Test(self):
        self.tester.assertEquals(
          self.policy_set,
          set(['Group1Policy1', 'Group1Policy2', 'Group2Policy3']))
    self.do_test(strings_mock, policies_mock, LocalMockWriter())

  def testPolicyTexts(self):
    # Test that GUI strings are assigned correctly to policies.
    strings_mock = {
      'IDS_POLICY_POLICY1_CAPTION': 'string1',
      'IDS_POLICY_POLICY1_DESC': 'string2',
      'IDS_POLICY_POLICY2_CAPTION': 'string3',
      'IDS_POLICY_POLICY2_DESC': 'string4',
      'IDS_POLICY_GROUP_GROUP1_CAPTION': None,
      'IDS_POLICY_GROUP_GROUP1_DESC': None,
    }
    policies_mock = {
      'Group1': {
        'policies': {
          'Policy1': {'type': 'string'},
          'Policy2': {'type': 'string'}
        }
      }
    }
    class LocalMockWriter(MockWriter):
      def WritePolicy(self, policy_name, policy):
        if policy_name == 'Policy1':
          self.tester.assertEquals(policy['caption'], 'string1')
          self.tester.assertEquals(policy['desc'], 'string2')
        elif policy_name == 'Policy2':
          self.tester.assertEquals(policy['caption'], 'string3')
          self.tester.assertEquals(policy['desc'], 'string4')
        else:
          self.tester.fail()
    self.do_test(strings_mock, policies_mock, LocalMockWriter())

  def testEnumTexts(self):
    # Test that GUI strings are assigned correctly to enums
    # (aka dropdown menus).
    strings_mock = {
      'IDS_POLICY_ENUM_ITEM1_CAPTION': 'string1',
      'IDS_POLICY_ENUM_ITEM2_CAPTION': 'string2',
      'IDS_POLICY_ENUM_ITEM3_CAPTION': 'string3',
      'IDS_POLICY_POLICY1_CAPTION': None,
      'IDS_POLICY_GROUP_GROUP1_CAPTION': None,
      'IDS_POLICY_GROUP_GROUP1_DESC': None,
    }
    policies_mock = {
      'Group1': {
        'policies': {
          'Policy1': {
            'type': 'enum',
            'items': {
              'item1': {'value': '0'},
              'item2': {'value': '1'},
              'item3': {'value': '3'},
            }
          }
        }
      }
    }
    class LocalMockWriter(MockWriter):
      def WritePolicy(self, policy_name, policy):
        self.tester.assertEquals(policy['items']['item1']['caption'], 'string1')
        self.tester.assertEquals(policy['items']['item2']['caption'], 'string2')
        self.tester.assertEquals(policy['items']['item3']['caption'], 'string3')
    self.do_test(strings_mock, policies_mock, LocalMockWriter())


if __name__ == '__main__':
  unittest.main()
