#!/usr/bin/python2.4
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Unittests for grit.format.policy_templates.writers.adml_writer."""


import os
import sys
import unittest
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../../../..'))


from grit.format.policy_templates.writers import adml_writer
from grit.format.policy_templates.writers import xml_writer_base_unittest
from xml.dom import minidom


class AdmlWriterTest(xml_writer_base_unittest.XmlWriterBaseTest):

  def setUp(self):
    config = {
      'build': 'test',
      'win_supported_os': 'SUPPORTED_TESTOS',
    }
    self.writer = adml_writer.GetWriter(config)
    self.writer.messages = {
      'win_supported_winxpsp2': {
        'text': 'Supported on Test OS or higher',
        'desc': 'blah'
      }
    }
    self.writer.Init()

  def _InitWriterForAddingPolicyGroups(self, writer):
    '''Initialize the writer for adding policy groups. This method must be
    called before the method "BeginPolicyGroup" can be called. It initializes
    attributes of the writer.
    '''
    writer.BeginTemplate()

  def _InitWriterForAddingPolicies(self, writer, policy):
    '''Initialize the writer for adding policies. This method must be
    called before the method "WritePolicy" can be called. It initializes
    attributes of the writer.
    '''
    self._InitWriterForAddingPolicyGroups(writer)
    policy_group = {
      'name': 'PolicyGroup',
      'caption': 'Test Caption',
      'desc': 'This is the test description of the test policy group.',
      'policies': policy,
    }
    writer.BeginPolicyGroup(policy_group)

    string_elements = \
        self.writer._string_table_elem.getElementsByTagName('string')
    for elem in string_elements:
      self.writer._string_table_elem.removeChild(elem)

  def testEmpty(self):
    self.writer.BeginTemplate()
    self.writer.EndTemplate()
    output = self.writer.GetTemplateText()
    expected_output = (
        '<?xml version="1.0" ?><policyDefinitionResources'
        ' revision="1.0" schemaVersion="1.0"><displayName/><description/>'
        '<resources><stringTable><string id="SUPPORTED_TESTOS">Supported on'
        ' Test OS or higher</string></stringTable><presentationTable/>'
        '</resources></policyDefinitionResources>')
    self.AssertXMLEquals(output, expected_output)

  def testPolicyGroup(self):
    empty_policy_group = {
      'name': 'PolicyGroup',
      'caption': 'Test Group Caption',
      'desc': 'This is the test description of the test policy group.',
      'policies': [
          {'name': 'PolicyStub2',
           'type': 'main'},
          {'name': 'PolicyStub1',
           'type': 'main'},
      ],
    }
    self._InitWriterForAddingPolicyGroups(self.writer)
    self.writer.BeginPolicyGroup(empty_policy_group)
    self.writer.EndPolicyGroup
    # Assert generated string elements.
    output = self.GetXMLOfChildren(self.writer._string_table_elem)
    expected_output = (
        '<string id="SUPPORTED_TESTOS">\n'
        '  Supported on Test OS or higher\n'
        '</string>\n'
        '<string id="PolicyGroup_group">\n'
        '  Test Group Caption\n'
        '</string>')
    self.AssertXMLEquals(output, expected_output)
    # Assert generated presentation elements.
    output = self.GetXMLOfChildren(self.writer._presentation_table_elem)
    expected_output = ''
    self.AssertXMLEquals(output, expected_output)

  def testMainPolicy(self):
    main_policy = {
      'name': 'DummyMainPolicy',
      'type': 'main',
      'caption': 'Main policy caption',
      'desc': 'Main policy test description.'
    }
    self. _InitWriterForAddingPolicies(self.writer, main_policy)
    self.writer.WritePolicy(main_policy)
    # Assert generated string elements.
    output = self.GetXMLOfChildren(self.writer._string_table_elem)
    expected_output = (
        '<string id="DummyMainPolicy">\n'
        '  Main policy caption\n'
        '</string>\n'
        '<string id="DummyMainPolicy_Explain">\n'
        '  Main policy test description.\n'
        '</string>')
    self.AssertXMLEquals(output, expected_output)
    # Assert generated presentation elements.
    output = self.GetXMLOfChildren(self.writer._presentation_table_elem)
    expected_output = '<presentation id="DummyMainPolicy"/>'
    self.AssertXMLEquals(output, expected_output)

  def testStringPolicy(self):
    string_policy = {
      'name': 'StringPolicyStub',
      'type': 'string',
      'caption': 'String policy caption',
      'label': 'String policy label',
      'desc': 'This is a test description.',
    }
    self. _InitWriterForAddingPolicies(self.writer, string_policy)
    self.writer.WritePolicy(string_policy)
    # Assert generated string elements.
    output = self.GetXMLOfChildren(self.writer._string_table_elem)
    expected_output = (
        '<string id="StringPolicyStub">\n'
        '  String policy caption\n'
        '</string>\n'
        '<string id="StringPolicyStub_Explain">\n'
        '  This is a test description.\n'
        '</string>')
    self.AssertXMLEquals(output, expected_output)
    # Assert generated presentation elements.
    output = self.GetXMLOfChildren(self.writer._presentation_table_elem)
    expected_output = (
        '<presentation id="StringPolicyStub">\n'
        '  <textBox refId="StringPolicyStub">\n'
        '    <label>\n'
        '      String policy label\n'
        '    </label>\n'
        '  </textBox>\n'
        '</presentation>')
    self.AssertXMLEquals(output, expected_output)

  def testIntPolicy(self):
    int_policy = {
      'name': 'IntPolicyStub',
      'type': 'int',
      'caption': 'Int policy caption',
      'label': 'Int policy label',
      'desc': 'This is a test description.',
    }
    self. _InitWriterForAddingPolicies(self.writer, int_policy)
    self.writer.WritePolicy(int_policy)
    # Assert generated string elements.
    output = self.GetXMLOfChildren(self.writer._string_table_elem)
    expected_output = (
        '<string id="IntPolicyStub">\n'
        '  Int policy caption\n'
        '</string>\n'
        '<string id="IntPolicyStub_Explain">\n'
        '  This is a test description.\n'
        '</string>')
    self.AssertXMLEquals(output, expected_output)
    # Assert generated presentation elements.
    output = self.GetXMLOfChildren(self.writer._presentation_table_elem)
    expected_output = (
        '<presentation id="IntPolicyStub">\n'
        '  <decimalTextBox refId="IntPolicyStub">\n'
        '    Int policy label:\n'
        '  </decimalTextBox>\n'
        '</presentation>')
    self.AssertXMLEquals(output, expected_output)

  def testIntEnumPolicy(self):
    enum_policy = {
      'name': 'EnumPolicyStub',
      'type': 'int-enum',
      'caption': 'Enum policy caption',
      'label': 'Enum policy label',
      'desc': 'This is a test description.',
      'items': [
          {
           'name': 'item 1',
           'value': 1,
           'caption': 'Caption Item 1',
          },
          {
           'name': 'item 2',
           'value': 2,
           'caption': 'Caption Item 2',
          },
      ],
    }
    self. _InitWriterForAddingPolicies(self.writer, enum_policy)
    self.writer.WritePolicy(enum_policy)
    # Assert generated string elements.
    output = self.GetXMLOfChildren(self.writer._string_table_elem)
    expected_output = (
        '<string id="EnumPolicyStub">\n'
        '  Enum policy caption\n'
        '</string>\n'
        '<string id="EnumPolicyStub_Explain">\n'
        '  This is a test description.\n'
        '</string>\n'
        '<string id="item 1">\n'
        '  Caption Item 1\n'
        '</string>\n'
        '<string id="item 2">\n'
        '  Caption Item 2\n'
        '</string>')
    self.AssertXMLEquals(output, expected_output)
    # Assert generated presentation elements.
    output = self.GetXMLOfChildren(self.writer._presentation_table_elem)
    expected_output = (
        '<presentation id="EnumPolicyStub">\n'
        '  <dropdownList refId="EnumPolicyStub">\n'
        '    Enum policy label\n'
        '  </dropdownList>\n'
        '</presentation>')
    self.AssertXMLEquals(output, expected_output)

  def testStringEnumPolicy(self):
    enum_policy = {
      'name': 'EnumPolicyStub',
      'type': 'string-enum',
      'caption': 'Enum policy caption',
      'label': 'Enum policy label',
      'desc': 'This is a test description.',
      'items': [
          {
           'name': 'item 1',
           'value': 'value 1',
           'caption': 'Caption Item 1',
          },
          {
           'name': 'item 2',
           'value': 'value 2',
           'caption': 'Caption Item 2',
          },
      ],
    }
    self. _InitWriterForAddingPolicies(self.writer, enum_policy)
    self.writer.WritePolicy(enum_policy)
    # Assert generated string elements.
    output = self.GetXMLOfChildren(self.writer._string_table_elem)
    expected_output = (
        '<string id="EnumPolicyStub">\n'
        '  Enum policy caption\n'
        '</string>\n'
        '<string id="EnumPolicyStub_Explain">\n'
        '  This is a test description.\n'
        '</string>\n'
        '<string id="item 1">\n'
        '  Caption Item 1\n'
        '</string>\n'
        '<string id="item 2">\n'
        '  Caption Item 2\n'
        '</string>')
    self.AssertXMLEquals(output, expected_output)
    # Assert generated presentation elements.
    output = self.GetXMLOfChildren(self.writer._presentation_table_elem)
    expected_output = (
        '<presentation id="EnumPolicyStub">\n'
        '  <dropdownList refId="EnumPolicyStub">\n'
        '    Enum policy label\n'
        '  </dropdownList>\n'
        '</presentation>')
    self.AssertXMLEquals(output, expected_output)

  def testListPolicy(self):
    list_policy = {
      'name': 'ListPolicyStub',
      'type': 'list',
      'caption': 'List policy caption',
      'label': 'List policy label',
      'desc': 'This is a test description.',
    }
    self. _InitWriterForAddingPolicies(self.writer, list_policy)
    self.writer.WritePolicy(list_policy)
    # Assert generated string elements.
    output = self.GetXMLOfChildren(self.writer._string_table_elem)
    expected_output = (
        '<string id="ListPolicyStub">\n'
        '  List policy caption\n'
        '</string>\n'
        '<string id="ListPolicyStub_Explain">\n'
        '  This is a test description.\n'
        '</string>\n'
        '<string id="ListPolicyStubDesc">\n'
        '  List policy caption\n'
        '</string>')
    self.AssertXMLEquals(output, expected_output)
    # Assert generated presentation elements.
    output = self.GetXMLOfChildren(self.writer._presentation_table_elem)
    expected_output = (
        '<presentation id="ListPolicyStub">\n'
        '  <listBox refId="ListPolicyStubDesc">\n'
        '    List policy label\n'
        '  </listBox>\n'
        '</presentation>')
    self.AssertXMLEquals(output, expected_output)

  def testPlatform(self):
    # Test that the writer correctly chooses policies of platform Windows.
    self.assertTrue(self.writer.IsPolicySupported({
      'supported_on': [
        {'platforms': ['win', 'zzz']}, {'platforms': ['aaa']}
      ]
    }))
    self.assertFalse(self.writer.IsPolicySupported({
      'supported_on': [
        {'platforms': ['mac', 'linux']}, {'platforms': ['aaa']}
      ]
    }))


if __name__ == '__main__':
  unittest.main()
