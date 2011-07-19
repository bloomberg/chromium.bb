#!/usr/bin/python2.4
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Class for reading GRD files into memory, without processing them.
'''

import os.path
import types
import xml.sax
import xml.sax.handler

from grit import exception
from grit.node import base
from grit.node import mapping
from grit.node import misc
from grit import util


class StopParsingException(Exception):
  '''An exception used to stop parsing.'''
  pass


class GrdContentHandler(xml.sax.handler.ContentHandler):
  def __init__(self,
      stop_after=None, debug=False, defines=None, tags_to_ignore=None):
    # Invariant of data:
    # 'root' is the root of the parse tree being created, or None if we haven't
    # parsed out any elements.
    # 'stack' is the a stack of elements that we push new nodes onto and
    # pop from when they finish parsing, or [] if we are not currently parsing.
    # 'stack[-1]' is the top of the stack.
    self.root = None
    self.stack = []
    self.stop_after = stop_after
    self.debug = debug
    self.defines = defines
    self.tags_to_ignore = tags_to_ignore or set()
    self.ignore_depth = 0

  def startElement(self, name, attrs):
    assert not self.root or len(self.stack) > 0

    if name in self.tags_to_ignore:
      self.ignore_depth += 1
      if self.debug:
        print "Ignoring element %s and its children" % name

    if self.ignore_depth != 0:
      return

    if self.debug:
      attr_list = []
      for attr in attrs.getNames():
        attr_list.append('%s="%s"' % (attr, attrs.getValue(attr)))
      if len(attr_list) == 0: attr_list = ['(none)']
      attr_list = ' '.join(attr_list)
      print ("Starting parsing of element %s with attributes %r" %
          (name, attr_list))

    typeattr = None
    if 'type' in attrs.getNames():
      typeattr = attrs.getValue('type')

    node = mapping.ElementToClass(name, typeattr)()

    if not self.root:
      self.root = node
      if self.defines:
        self.root.SetDefines(self.defines)

    if len(self.stack) > 0:
      self.stack[-1].AddChild(node)
      node.StartParsing(name, self.stack[-1])
    else:
      node.StartParsing(name, None)

    # Push
    self.stack.append(node)

    for attr in attrs.getNames():
      node.HandleAttribute(attr, attrs.getValue(attr))

  def endElement(self, name):
    if self.ignore_depth == 0:
      if self.debug:
        print "End parsing of element %s" % name
      # Pop
      self.stack[-1].EndParsing()
      assert len(self.stack) > 0
      self.stack = self.stack[:-1]
      if self.stop_after and name == self.stop_after:
        raise StopParsingException()

    if name in self.tags_to_ignore:
      self.ignore_depth -= 1

  def characters(self, content):
    if self.ignore_depth == 0:
      if self.stack[-1]:
        self.stack[-1].AppendContent(content)

  def ignorableWhitespace(self, whitespace):
    # TODO(joi)  This is not supported by expat.  Should use a different XML parser?
    pass


def Parse(filename_or_stream, dir=None, flexible_root=False,
          stop_after=None, debug=False, first_id_filename=None,
          defines=None, tags_to_ignore=None):
  '''Parses a GRD file into a tree of nodes (from grit.node).

  If flexible_root is False, the root node must be a <grit> element.  Otherwise
  it can be any element.  The "own" directory of the file will only be fixed up
  if the root node is a <grit> element.

  'dir' should point to the directory of the input file, or be the full path
  to the input file (the filename will be stripped).

  If 'stop_after' is provided, the parsing will stop once the first node
  with this name has been fully parsed (including all its contents).

  If 'debug' is true, lots of information about the parsing events will be
  printed out during parsing of the file.

  If first_id_filename is provided, then we use the provided path instead of
  resources_id to gather the first id values for resources.

  Args:
    filename_or_stream: './bla.xml'  (must be filename if dir is None)
    dir: '.' or None (only if filename_or_stream is a filename)
    flexible_root: True | False
    stop_after: 'inputs'
    debug: False
    first_id_filename: None
    defines: dictionary of defines, like {'chromeos': '1'}

  Return:
    Subclass of grit.node.base.Node

  Throws:
    grit.exception.Parsing
  '''
  handler = GrdContentHandler(stop_after=stop_after, debug=debug,
                              defines=defines, tags_to_ignore=tags_to_ignore)
  try:
    xml.sax.parse(filename_or_stream, handler)
  except StopParsingException:
    assert stop_after
    pass
  except:
    if not debug:
      print "parse exception: run GRIT with the -x flag to debug .grd problems"
    raise

  if not flexible_root or hasattr(handler.root, 'SetOwnDir'):
    assert isinstance(filename_or_stream, types.StringType) or dir != None
    if not dir:
      dir = util.dirname(filename_or_stream)
      if len(dir) == 0:
        dir = '.'
    # Fix up the base_dir so it is relative to the input file.
    handler.root.SetOwnDir(dir)

  # Assign first ids to the nodes that don't have them.
  if isinstance(handler.root, misc.GritNode) and first_id_filename != '':
    handler.root.AssignFirstIds(filename_or_stream, first_id_filename, defines)

  return handler.root


if __name__ == '__main__':
  util.ChangeStdoutEncoding()
  print unicode(Parse(sys.argv[1]))
