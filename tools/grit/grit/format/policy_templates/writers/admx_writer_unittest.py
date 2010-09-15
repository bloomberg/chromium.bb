#!/usr/bin/python2.4
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Unittests for grit.format.policy_templates.writers.admx_writer."""


import os
import sys
import unittest
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../../../..'))


from grit.format.policy_templates.writers import admx_writer
from grit.format.policy_templates.writers import xml_writer_base_unittest
from xml.dom import minidom


class AdmxWriterTest(xml_writer_base_unittest.XmlWriterBaseTest):

  def _CreateDocumentElement(self):
    dom_impl = minidom.getDOMImplementation('')
    doc = dom_impl.createDocument(None, 'root', None)
    return doc.documentElement

  def setUp(self):
    # Writer configuration. This dictionary contains parameter used by the ADMX
    # Writer
    config =  {
      'win_group_policy_class': 'TestClass',
      'win_supported_os': 'SUPPORTED_TESTOS',
      'win_supported_os_msg': 'IDS_POLICY_WIN_SUPPORTED_WINXPSP2',
      'win_reg_key_name': 'Software\\Policies\\Test',
      'win_category_path': ['test_category'],
      'admx_namespace': 'ADMXWriter.Test.Namespace',
      'admx_prefix': 'test_prefix'
    }
    # Grit messages.
    messages = {}
    self.writer = admx_writer.GetWriter(config, messages)
    self.writer.Prepare()

  def testEmpty(self):
    self.writer.BeginTemplate()
    self.writer.EndTemplate()

    output = self.writer.GetTemplateText()
    expected_output = (
        '<?xml version="1.0" ?>\n<policyDefinitions revision="1.0"'
        ' schemaVersion="1.0">\n  <policyNamespaces>\n    <target'
        ' namespace="ADMXWriter.Test.Namespace" prefix="test_prefix"/>\n'
        '    <using namespace="Microsoft.Policies.Windows" prefix="windows"/>'
        '\n  </policyNamespaces>\n  <resources'
        ' minRequiredRevision="1.0"/>\n  <supportedOn>\n'
        '    <definitions>\n      <definition displayName="'
        '$(string.SUPPORTED_TESTOS)" name="SUPPORTED_TESTOS"/>\n'
        '    </definitions>\n  </supportedOn>\n  <categories>\n    <category'
        ' displayName="$(string.test_category)" name="test_category"/>\n'
        '  </categories>\n  <policies/>\n</policyDefinitions>')
    self.AssertXMLEquals(output, expected_output)

  def testPolicyGroup(self):
    parent_elem = self._CreateDocumentElement()
    self.writer._active_policies_elem = parent_elem

    empty_policy_group = {
      'name': 'PolicyGroup'
    }
    self.writer.BeginPolicyGroup(empty_policy_group)
    self.writer.EndPolicyGroup()

    output = self.GetXMLOfChildren(parent_elem)
    expected_output = (
        '<policy class="TestClass" displayName="$(string.PolicyGroup)"'
        ' explainText="$(string.PolicyGroup_Explain)" key='
        '"Software\Policies\Test" name="PolicyGroup"'
        ' presentation="$(presentation.PolicyGroup)" valueName="PolicyGroup">'
        '\n  <parentCategory ref="test_category"/>\n'
        '  <supportedOn ref="SUPPORTED_TESTOS"/>\n</policy>')
    self.AssertXMLEquals(output, expected_output)

  def testMainPolicy(self):
    main_policy = {
      'name': 'DummyMainPolicy',
      'type': 'main',
    }

    parent_elem = self._CreateDocumentElement()
    self.writer._active_policy_elem = parent_elem

    self.writer.WritePolicy(main_policy)

    output = self.GetXMLOfChildren(parent_elem)
    expected_output = (
        '<enabledValue>\n  <decimal value="1"/>\n</enabledValue>\n'
        '<disabledValue>\n  <decimal value="0"/>\n</disabledValue>')
    self.AssertXMLEquals(output, expected_output)

  def testStringPolicy(self):
    string_policy = {
      'name': 'SampleStringPolicy',
      'type': 'string',
    }
    parent_elem = self.writer.AddElement(self._CreateDocumentElement(),
                                           'policy')
    self.writer._active_policy_elem = parent_elem
    self.writer.WritePolicy(string_policy)
    output = self.GetXMLOfChildren(parent_elem)
    expected_output = (
        '<elements>\n  <text id="SampleStringPolicy"'
        ' valueName="SampleStringPolicy"/>\n</elements>')
    self.AssertXMLEquals(output, expected_output)

  def testEnumPolicy(self):
    parent_elem = self.writer.AddElement(self._CreateDocumentElement(),
                                           'policy')
    self.writer._active_policy_elem = parent_elem

    enum_policy = {
      'name': 'SampleEnumPolicy',
      'type': 'enum',
        'items': [
          {'name': 'item 1', 'value': '0'},
          {'name': 'item 2', 'value': '1'},
        ]
    }
    self.writer.WritePolicy(enum_policy)
    output = self.GetXMLOfChildren(parent_elem)
    expected_output = (
        '<elements>\n  <enum id="SampleEnumPolicy" valueName='
        '"SampleEnumPolicy">\n    <item displayName="$(string.item 1)">\n'
        '      <value>\n        <decimal value="0"/>\n      </value>\n'
        '    </item>\n    <item displayName="$(string.item 2)">\n      <value>'
        '\n        <decimal value="1"/>\n      </value>\n    </item>'
        '\n  </enum>\n</elements>')
    self.AssertXMLEquals(output, expected_output)

  def testListPolicy(self):
    list_policy = {
      'name': 'SampleListPolicy',
      'type': 'list',
    }
    parent_elem = self.writer.AddElement(self._CreateDocumentElement(),
                                         'policy')
    self.writer._active_policy_elem = parent_elem
    self.writer.WritePolicy(list_policy)
    output = self.GetXMLOfChildren(parent_elem)
    expected_output = (
        '<elements>\n  <list id="SampleListPolicyDesc"'
        ' key="Software\Policies\Test\SampleListPolicy"'
        ' valuePrefix=""/>\n</elements>')

    self.AssertXMLEquals(output, expected_output)

  def testPlatform(self):
    # Test that the writer correctly chooses policies of platform Windows.
    self.assertTrue(self.writer.IsPolicySupported({
      'annotations': {
        'platforms': ['win', 'zzz', 'aaa']
      }
    }))
    self.assertFalse(self.writer.IsPolicySupported({
      'annotations': {
        'platforms': ['mac', 'linux', 'aaa']
      }
    }))


if __name__ == '__main__':
  unittest.main()
