#!/usr/bin/python2.4
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for misc.GritNode'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../..'))

import unittest
import StringIO

from grit import grd_reader
import grit.exception
from grit import util
from grit.node import misc


class GritNodeUnittest(unittest.TestCase):
  def testUniqueNameAttribute(self):
    try:
      restree = grd_reader.Parse(
        util.PathFromRoot('grit/testdata/duplicate-name-input.xml'))
      self.fail('Expected parsing exception because of duplicate names.')
    except grit.exception.Parsing:
      pass  # Expected case

  def testReadFirstIdsFromFile(self):
    test_resource_ids = os.path.join(os.path.dirname(__file__), '..',
                                     'testdata', 'resource_ids')
    src_dir, id_dict = misc._ReadFirstIdsFromFile(
        test_resource_ids,
        {
          'FOO': 'bar',
          'SHARED_INTERMEDIATE_DIR': 'out/Release/obj/gen',
        })
    self.assertEqual({}, id_dict.get('bar/file.grd', None))
    self.assertEqual({},
        id_dict.get('out/Release/obj/gen/devtools/devtools.grd', None))


class IfNodeUnittest(unittest.TestCase):
  def testIffyness(self):
    grd = grd_reader.Parse(StringIO.StringIO('''
      <grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
        <release seq="3">
          <messages>
            <if expr="'bingo' in defs">
              <message name="IDS_BINGO">
                Bingo!
              </message>
            </if>
            <if expr="'hello' in defs">
              <message name="IDS_HELLO">
                Hello!
              </message>
            </if>
            <if expr="lang == 'fr' or 'FORCE_FRENCH' in defs">
              <message name="IDS_HELLO" internal_comment="French version">
                Good morning
              </message>
            </if>
          </messages>
        </release>
      </grit>'''), dir='.')

    messages_node = grd.children[0].children[0]
    bingo_message = messages_node.children[0].children[0]
    hello_message = messages_node.children[1].children[0]
    french_message = messages_node.children[2].children[0]
    self.assertTrue(bingo_message.name == 'message')
    self.assertTrue(hello_message.name == 'message')
    self.assertTrue(french_message.name == 'message')

    grd.SetOutputContext('fr', {'hello' : '1'})
    self.failUnless(not bingo_message.SatisfiesOutputCondition())
    self.failUnless(hello_message.SatisfiesOutputCondition())
    self.failUnless(french_message.SatisfiesOutputCondition())

    grd.SetOutputContext('en', {'bingo' : 1})
    self.failUnless(bingo_message.SatisfiesOutputCondition())
    self.failUnless(not hello_message.SatisfiesOutputCondition())
    self.failUnless(not french_message.SatisfiesOutputCondition())

    grd.SetOutputContext('en', {'FORCE_FRENCH' : '1', 'bingo' : '1'})
    self.failUnless(bingo_message.SatisfiesOutputCondition())
    self.failUnless(not hello_message.SatisfiesOutputCondition())
    self.failUnless(french_message.SatisfiesOutputCondition())

  def testIffynessWithOutputNodes(self):
    grd = grd_reader.Parse(StringIO.StringIO('''
      <grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
        <outputs>
          <output filename="uncond1.rc" type="rc_data" />
          <if expr="lang == 'fr' or 'hello' in defs">
            <output filename="only_fr.adm" type="adm" />
            <output filename="only_fr.plist" type="plist" />
          </if>
          <if expr="lang == 'ru'">
            <output filename="doc.html" type="document" />
          </if>
          <output filename="uncond2.adm" type="adm" />
          <output filename="iftest.h" type="rc_header">
            <emit emit_type='prepend'></emit>
          </output>
        </outputs>
      </grit>'''), dir='.')

    outputs_node = grd.children[0]
    uncond1_output = outputs_node.children[0]
    only_fr_adm_output = outputs_node.children[1].children[0]
    only_fr_plist_output = outputs_node.children[1].children[1]
    doc_output = outputs_node.children[2].children[0]
    uncond2_output = outputs_node.children[0]
    self.assertTrue(uncond1_output.name == 'output')
    self.assertTrue(only_fr_adm_output.name == 'output')
    self.assertTrue(only_fr_plist_output.name == 'output')
    self.assertTrue(doc_output.name == 'output')
    self.assertTrue(uncond2_output.name == 'output')

    grd.SetOutputContext('ru', {'hello' : '1'})
    outputs = [output.GetFilename() for output in grd.GetOutputFiles()]
    self.assertEquals(
        outputs,
        ['uncond1.rc', 'only_fr.adm', 'only_fr.plist', 'doc.html',
         'uncond2.adm', 'iftest.h'])

    grd.SetOutputContext('ru', {'bingo': '2'})
    outputs = [output.GetFilename() for output in grd.GetOutputFiles()]
    self.assertEquals(
        outputs,
        ['uncond1.rc', 'doc.html', 'uncond2.adm', 'iftest.h'])

    grd.SetOutputContext('fr', {'hello': '1'})
    outputs = [output.GetFilename() for output in grd.GetOutputFiles()]
    self.assertEquals(
        outputs,
        ['uncond1.rc', 'only_fr.adm', 'only_fr.plist', 'uncond2.adm',
         'iftest.h'])

    grd.SetOutputContext('en', {'bingo': '1'})
    outputs = [output.GetFilename() for output in grd.GetOutputFiles()]
    self.assertEquals(outputs, ['uncond1.rc', 'uncond2.adm', 'iftest.h'])

    grd.SetOutputContext('fr', {'bingo': '1'})
    outputs = [output.GetFilename() for output in grd.GetOutputFiles()]
    self.assertNotEquals(outputs, ['uncond1.rc', 'uncond2.adm', 'iftest.h'])

class ReleaseNodeUnittest(unittest.TestCase):
  def testPseudoControl(self):
    grd = grd_reader.Parse(StringIO.StringIO('''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="1" source_lang_id="en-US" current_release="2" base_dir=".">
        <release seq="1" allow_pseudo="false">
          <messages>
            <message name="IDS_HELLO">
              Hello
            </message>
          </messages>
          <structures>
            <structure type="dialog" name="IDD_ABOUTBOX" encoding="utf-16" file="klonk.rc" />
          </structures>
        </release>
        <release seq="2">
          <messages>
            <message name="IDS_BINGO">
              Bingo
            </message>
          </messages>
          <structures>
            <structure type="menu" name="IDC_KLONKMENU" encoding="utf-16" file="klonk.rc" />
          </structures>
        </release>
      </grit>'''), util.PathFromRoot('grit/testdata'))
    grd.RunGatherers(recursive=True)

    hello = grd.GetNodeById('IDS_HELLO')
    aboutbox = grd.GetNodeById('IDD_ABOUTBOX')
    bingo = grd.GetNodeById('IDS_BINGO')
    menu = grd.GetNodeById('IDC_KLONKMENU')

    for node in [hello, aboutbox]:
      self.failUnless(not node.PseudoIsAllowed())

    for node in [bingo, menu]:
      self.failUnless(node.PseudoIsAllowed())

    for node in [hello, aboutbox]:
      try:
        formatter = node.ItemFormatter('rc_all')
        formatter.Format(node, 'xyz-pseudo')
        self.fail('Should have failed during Format since pseudo is not allowed')
      except:
        pass  # expected case

    for node in [bingo, menu]:
      try:
        formatter = node.ItemFormatter('rc_all')
        formatter.Format(node, 'xyz-pseudo')
      except:
        self.fail('Should not have gotten exception since pseudo is allowed')


if __name__ == '__main__':
  unittest.main()
