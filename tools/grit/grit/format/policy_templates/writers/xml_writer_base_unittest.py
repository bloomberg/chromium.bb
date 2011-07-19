#!/usr/bin/python2.4
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Unittests for grit.format.policy_templates.writers.admx_writer."""


import os
import sys
import unittest


from xml.dom import minidom


class XmlWriterBaseTest(unittest.TestCase):
  '''Base class for XML writer unit-tests.
  '''

  def GetXMLOfChildren(self, parent):
    '''Returns the XML of all child nodes of the given parent node.
    Args:
      parent: The XML of the children of this node will  be returned.

    Return: XML of the chrildren of the parent node.
    '''
    return ''.join(
      child.toprettyxml(indent='  ') for child in parent.childNodes)

  def AssertXMLEquals(self, output, expected_output):
    '''Asserts if the passed XML arguements are equal.
    Args:
      output: Actual XML text.
      expected_output: Expected XML text.
    '''
    self.assertEquals(output.strip(), expected_output.strip())
