#!/usr/bin/python2.4
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittest for js_map_format.py.
"""

import os
import re
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../..'))

import unittest
import StringIO

from grit import grd_reader
from grit import util
from grit.tool import build


class JsMapFormatUnittest(unittest.TestCase):

  def testMessages(self):
    grd_text = u"""
    <messages>
      <message name="IDS_SIMPLE_MESSAGE">
              Simple message.
      </message>
      <message name="IDS_QUOTES">
              element\u2019s \u201c<ph name="NAME">%s<ex>name</ex></ph>\u201d attribute
      </message>
      <message name="IDS_PLACEHOLDERS">
              <ph name="ERROR_COUNT">%1$d<ex>1</ex></ph> error, <ph name="WARNING_COUNT">%2$d<ex>1</ex></ph> warning
      </message>
      <message name="IDS_STARTS_WITH_SPACE">
              ''' (<ph name="COUNT">%d<ex>2</ex></ph>)
      </message>
      <message name="IDS_DOUBLE_QUOTES">
              A "double quoted" message.
      </message>
    </messages>
    """
    root = grd_reader.Parse(StringIO.StringIO(grd_text.encode('utf-8')),
                            flexible_root=True)
    util.FixRootForUnittest(root)

    buf = StringIO.StringIO()
    build.RcBuilder.ProcessNode(root, DummyOutput('js_map_format', 'en'), buf)
    output = buf.getvalue()
    test = u"""
localizedStrings["Simple message."] = "Simple message.";
localizedStrings["element\u2019s \u201c%s\u201d attribute"] = "element\u2019s \u201c%s\u201d attribute";
localizedStrings["%d error, %d warning"] = "%1$d error, %2$d warning";
localizedStrings[" (%d)"] = " (%d)";
localizedStrings["A \\\"double quoted\\\" message."] = "A \\\"double quoted\\\" message.";
"""
    self.failUnless(output.strip() == test.strip())

  def testTranslations(self):
    root = grd_reader.Parse(StringIO.StringIO("""
    <messages>
        <message name="ID_HELLO">Hello!</message>
        <message name="ID_HELLO_USER">Hello <ph name="USERNAME">%s<ex>
          Joi</ex></ph></message>
      </messages>
    """), flexible_root=True)
    util.FixRootForUnittest(root)

    buf = StringIO.StringIO()
    build.RcBuilder.ProcessNode(root, DummyOutput('js_map_format', 'fr'), buf)
    output = buf.getvalue()
    test = u"""
localizedStrings["Hello!"] = "H\xe9P\xe9ll\xf4P\xf4!";
localizedStrings["Hello %s"] = "H\xe9P\xe9ll\xf4P\xf4 %s";
"""
    self.failUnless(output.strip() == test.strip())


class DummyOutput(object):

  def __init__(self, type, language):
    self.type = type
    self.language = language

  def GetType(self):
    return self.type

  def GetLanguage(self):
    return self.language

  def GetOutputFilename(self):
    return 'hello.gif'

if __name__ == '__main__':
  unittest.main()
