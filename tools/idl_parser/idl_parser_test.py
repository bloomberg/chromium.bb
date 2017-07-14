#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import unittest

from idl_lexer import IDLLexer
from idl_parser import IDLParser, ParseFile

def ParseCommentTest(comment):
  comment = comment.strip()
  comments = comment.split(None, 1)
  return comments[0], comments[1]


class WebIDLParser(unittest.TestCase):
  def setUp(self):
    self.parser = IDLParser(IDLLexer(), mute_error=True)
    self.filenames = glob.glob('test_parser/*_web.idl')

  def _TestNode(self, node):
    comments = node.GetListOf('SpecialComment')
    for comment in comments:
      check, value = ParseCommentTest(comment.GetName())
      if check == 'BUILD':
        msg = 'Expecting %s, but found %s.\n' % (value, str(node))
        self.assertEqual(value, str(node), msg)

      if check == 'ERROR':
        msg = node.GetLogLine('Expecting\n\t%s\nbut found \n\t%s\n' % (
                              value, str(node)))
        self.assertEqual(value, node.GetName(), msg)

      if check == 'PROP':
        key, expect = value.split('=')
        actual = str(node.GetProperty(key))
        msg = 'Mismatched property %s: %s vs %s.\n' % (key, expect, actual)
        self.assertEqual(expect, actual, msg)

      if check == 'TREE':
        quick = '\n'.join(node.Tree())
        lineno = node.GetProperty('LINENO')
        msg = 'Mismatched tree at line %d:\n%sVS\n%s' % (lineno, value, quick)
        self.assertEqual(value, quick, msg)

  def testExpectedNodes(self):
    for filename in self.filenames:
      filenode = ParseFile(self.parser, filename)
      children = filenode.GetChildren()
      self.assertTrue(len(children) > 2, 'Expecting children in %s.' %
                      filename)

      for node in filenode.GetChildren():
        self._TestNode(node)


if __name__ == '__main__':
  unittest.main(verbosity=2)
