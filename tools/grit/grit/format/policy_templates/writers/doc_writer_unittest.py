# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.format.policy_templates.writers.doc_writer'''


import os
import re
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../../../..'))

import tempfile
import unittest
import StringIO
from xml.dom import minidom

from grit.format import rc
from grit.format.policy_templates.writers import writer_unittest_common
from grit.format.policy_templates.writers import doc_writer
from grit import grd_reader
from grit import util
from grit.tool import build

class MockMessageDictionary:
  '''A mock dictionary passed to a writer as the dictionary of
  localized messages.
  '''

  # Dictionary of messages.
  msg_dict = {}

  def __getitem__(self, msg_id):
    '''Returns a message for an identifier. The identifier is transformed
    back from IDS_POLICY_DOC... style names to the keys that the writer used.
    If it is then  key in self.msg_dict, then the message comes from there.
    Otherwise the returned message is just the transformed version of the
    identifier. This makes things more simple for testing.

    Args:
      msg_id: The message identifier.

    Returns:
      The mock message for msg_id.
    '''
    # Do some trickery to get the original message id issued in DocWriter.
    expected_prefix = 'IDS_POLICY_DOC_'
    assert msg_id.startswith(expected_prefix)
    original_msg_id = msg_id[len(expected_prefix):].lower()

    if original_msg_id in self.msg_dict:
      return self.msg_dict[original_msg_id]
    return '_test_' + original_msg_id


class DocWriterUnittest(writer_unittest_common.WriterUnittestCommon):
  '''Unit tests for DocWriter.'''

  def setUp(self):
    # Create a writer for the tests.
    self.writer = doc_writer.GetWriter(
      config={
        'app_name': 'Chrome',
        'frame_name': 'Chrome Frame',
        'win_reg_key_name': 'MockKey',
      },
      messages=MockMessageDictionary())
    self.writer.Init()

    # It is not worth testing the exact content of style attributes.
    # Therefore we override them here with shorter texts.
    for key in self.writer._STYLE.keys():
      self.writer._STYLE[key] = 'style_%s;' % key
    # Add some more style attributes for additional testing.
    self.writer._STYLE['key1'] = 'style1;'
    self.writer._STYLE['key2'] = 'style2;'

    # Create a DOM document for the tests.
    dom_impl = minidom.getDOMImplementation('')
    self.doc = dom_impl.createDocument(None, 'root', None)
    self.doc_root = self.doc.documentElement

  def testSkeleton(self):
    # Test if DocWriter creates the skeleton of the document correctly.
    self.writer.BeginTemplate()
    self.assertEquals(
        self.writer._main_div.toxml(),
        '<div>'
          '<div>'
            '<a name="top"/><br/>_test_intro<br/><br/><br/>'
            '<table style="style_table;">'
              '<thead><tr style="style_tr;">'
                '<td style="style_td;style_td.left;style_thead td;">'
                  '_test_name_column_title'
                '</td>'
                '<td style="style_td;style_td.right;style_thead td;">'
                  '_test_description_column_title'
                '</td>'
              '</tr></thead>'
              '<tbody/>'
            '</table>'
          '</div>'
          '<div/>'
        '</div>')

  def testGetLocalizedPolicyMessage(self):
    # Test if the message inheritance logic works well in
    # DocWriter.GetLocalizedPolicyMessage()
    policy = {'message_id1': '1hello, world'}
    self.assertEquals(
        self.writer._GetLocalizedPolicyMessage(policy, 'message_id1'),
        '1hello, world')
    self.assertRaises(
        KeyError,
        self.writer._GetLocalizedPolicyMessage, policy, 'message_id2')
    policy['parent'] = {'message_id3': '3hello, world'}
    self.assertEquals(
        self.writer._GetLocalizedPolicyMessage(policy, 'message_id3'),
        '3hello, world')

  def testGetLocalizedMessage(self):
    # Test if localized messages are retrieved correctly.
    self.writer.messages = {
      'IDS_POLICY_DOC_HELLO_WORLD': 'hello, vilag!'
    }
    self.assertEquals(
        self.writer._GetLocalizedMessage('hello_world'),
        'hello, vilag!')

  def testMapListToString(self):
    # Test function DocWriter.MapListToString()
    self.assertEquals(
        self.writer._MapListToString({'a1': 'a2', 'b1': 'b2'}, ['a1', 'b1']),
        'a2, b2')
    self.assertEquals(
        self.writer._MapListToString({'a1': 'a2', 'b1': 'b2'}, []),
        '')
    result = self.writer._MapListToString(
        {'a': '1', 'b': '2', 'c': '3', 'd': '4'}, ['b', 'd'])
    expected_result = '2, 4'
    self.assertEquals(
        result,
        expected_result)

  def testAddStyledElement(self):
    # Test function DocWriter.AddStyledElement()

    # Test the case of zero style.
    e1 = self.writer._AddStyledElement(
        self.doc_root, 'z', [], {'a': 'b'}, 'text')
    self.assertEquals(
        e1.toxml(),
        '<z a="b">text</z>')

    # Test the case of one style.
    e2 = self.writer._AddStyledElement(
        self.doc_root, 'z', ['key1'], {'a': 'b'}, 'text')
    self.assertEquals(
        e2.toxml(),
        '<z a="b" style="style1;">text</z>')

    # Test the case of two styles.
    e3 = self.writer._AddStyledElement(
        self.doc_root, 'z', ['key1', 'key2'], {'a': 'b'}, 'text')
    self.assertEquals(
        e3.toxml(),
        '<z a="b" style="style1;style2;">text</z>')

  def testAddDescription(self):
    # Test if URLs are replaced and choices of 'enum' policies are listed
    # correctly.
    policy = {
      'type': 'enum',
      'items': [
        {'value': '0', 'caption': 'Disable foo'},
        {'value': '2', 'caption': 'Solve your problem'},
        {'value': '5', 'caption': 'Enable bar'},
      ],
      'desc': '''This policy disables foo, except in case of bar.
See http://policy-explanation.example.com for more details.
'''
    }
    self.writer._AddDescription(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(),
        '''<root>This policy disables foo, except in case of bar.
See <a href="http://policy-explanation.example.com">http://policy-explanation.example.com</a> for more details.
<ul><li>0 = Disable foo</li><li>2 = Solve your problem</li><li>5 = Enable bar</li></ul></root>''')

  def testAddFeatures(self):
    # Test if the list of features of a policy is handled correctly.
    policy = {
      'annotations': {
        'features': {'spaceship_docking': 0, 'dynamic_refresh': 1}
      }
    }
    self.writer._FEATURE_MAP = {
      'spaceship_docking': 'Spaceship Docking',
      'dynamic_refresh': 'Dynamic Refresh'
    }
    self.writer._AddFeatures(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(),
        '<root>'
          'Dynamic Refresh: _test_supported, '
          'Spaceship Docking: _test_not_supported'
        '</root>')

  def testAddListExample(self):
    policy = {
      'name': 'PolicyName',
      'annotations': {
        'example_value': ['Foo', 'Bar']
      }
    }
    self.writer._AddListExample(self.doc_root, policy)
    self.assertEquals(
      self.doc_root.toxml(),
      '''<root><dl style="style_dd dl;"><dt>Windows:</dt><dd style="style_.monospace;style_.pre;">MockKey\\PolicyName\\0 = &quot;Foo&quot;
MockKey\\PolicyName\\1 = &quot;Bar&quot;</dd><dt>Linux:</dt><dd style="style_.monospace;">[&quot;Foo&quot;, &quot;Bar&quot;]</dd><dt>Mac:</dt><dd style="style_.monospace;style_.pre;">&lt;array&gt;
  &lt;string&gt;Foo&lt;/string&gt;
  &lt;string&gt;Bar&lt;/string&gt;
&lt;/array&gt;</dd></dl></root>''')

  def testBoolExample(self):
    # Test representation of boolean example values.
    policy = {
      'name': 'PolicyName',
      'type': 'main',
      'annotations': {
        'example_value': True
      }
    }
    e1 = self.writer.AddElement(self.doc_root, 'e1')
    self.writer._AddExample(e1, policy)
    self.assertEquals(
        e1.toxml(),
        '<e1>0x00000001 (Windows), true (Linux), &lt;true /&gt; (Mac)</e1>')

    policy = {
      'name': 'PolicyName',
      'type': 'main',
      'annotations': {
        'example_value': False
      }
    }
    e2 = self.writer.AddElement(self.doc_root, 'e2')
    self.writer._AddExample(e2, policy)
    self.assertEquals(
        e2.toxml(),
        '<e2>0x00000000 (Windows), false (Linux), &lt;false /&gt; (Mac)</e2>')

  def testEnumExample(self):
    # Test representation of 'enum' example values.
    policy = {
      'name': 'PolicyName',
      'type': 'enum',
      'annotations': {
        'example_value': 16
      }
    }
    self.writer._AddExample(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(),
        '<root>0x00000010 (Windows), 16 (Linux/Mac)</root>')

  def testStringExample(self):
    # Test representation of 'string' example values.
    policy = {
      'name': 'PolicyName',
      'type': 'string',
      'annotations': {
        'example_value': 'awesome-example'
      }
    }
    self.writer._AddExample(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(),
        '<root>&quot;awesome-example&quot;</root>')

  def testAddPolicyAttribute(self):
    # Test creating a policy attribute term-definition pair.
    self.writer._AddPolicyAttribute(
        self.doc_root, 'bla', 'hello, world', ['key1'])
    self.assertEquals(
        self.doc_root.toxml(),
        '<root>'
          '<dt style="style_dt;">_test_bla</dt>'
          '<dd style="style1;">hello, world</dd>'
        '</root>')

  def testAddPolicyDetails(self):
    # Test if the definition list (<dl>) of policy details is created correctly.
    policy = {
      'type': 'main',
      'name': 'TestPolicyName',
      'caption': 'TestPolicyCaption',
      'desc': 'TestPolicyDesc',
      'annotations': {
        'platforms': ['win'],
        'products': ['chrome'],
        'features': {'dynamic_refresh': 0},
        'example_value': False
      }
    }
    self.writer._AddPolicyDetails(self.doc_root, policy)
    self.assertEquals(
      self.doc_root.toxml(),
      '<root><dl>'
      '<dt style="style_dt;">_test_data_type</dt><dd>Boolean (REG_DWORD)</dd>'
      '<dt style="style_dt;">_test_win_reg_loc</dt>'
      '<dd style="style_.monospace;">MockKey\TestPolicyName</dd>'
      '<dt style="style_dt;">_test_mac_linux_pref_name</dt>'
        '<dd style="style_.monospace;">TestPolicyName</dd>'
      '<dt style="style_dt;">_test_supported_on_platforms</dt><dd>Windows</dd>'
      '<dt style="style_dt;">_test_supported_in_products</dt><dd>Chrome</dd>'
      '<dt style="style_dt;">_test_supported_features</dt>'
        '<dd>Dynamic Policy Refresh: _test_not_supported</dd>'
      '<dt style="style_dt;">_test_description</dt><dd>TestPolicyDesc</dd>'
      '<dt style="style_dt;">_test_example_value</dt>'
        '<dd>0x00000000 (Windows), false (Linux), &lt;false /&gt; (Mac)</dd>'
      '</dl></root>')

  def testAddPolicyNote(self):
    # Test if nodes are correctly added to policies.
    policy = {
      'annotations': {
        'problem_href': 'http://www.example.com/5'
      }
    }
    self.writer.messages.msg_dict['note'] = '...$6...'
    self.writer._AddPolicyNote(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(),
        '<root><div style="style_div.note;">...'
        '<a href="http://www.example.com/5">http://www.example.com/5</a>'
        '...</div></root>')

  def testAddPolicyRow(self):
    # Test if policies are correctly added to the summary table.
    policy = {
      'name': 'PolicyName',
      'caption': 'PolicyCaption'
    }
    self.writer._AddPolicyRow(self.doc_root, policy)
    self.assertEquals(
      self.doc_root.toxml(),
      '<root><tr style="style_tr;">'
      '<td style="style_td;style_td.left;">'
        '<a href="#PolicyName">PolicyName</a>'
      '</td>'
      '<td style="style_td;style_td.right;">PolicyCaption</td>'
      '</tr></root>')


if __name__ == '__main__':
  unittest.main()
