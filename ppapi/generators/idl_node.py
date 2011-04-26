#!/usr/bin/python
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Nodes for PPAPI IDL AST """

#
# IDL Node
#
# IDL Node defines the IDLAttribute and IDLNode objects which are constructed
# by the parser as it processes the various 'productions'.  The IDLAttribute
# objects are assigned to the IDLNode's property dictionary instead of being
# applied as children of The IDLNodes, so they do not exist in the final tree.
# The AST of IDLNodes is the output from the parsing state and will be used
# as the source data by the various generators.
#

import sys
import hashlib

from idl_log import ErrOut, InfoOut, WarnOut

#
# IDLAttribute
#
# A temporary object used by the parsing process to hold an Extended Attribute
# which will be passed as a child to a standard IDLNode.
#
class IDLAttribute(object):
  def __init__(self, name, value):
    self.cls = 'ExtAttribute'
    self.name = name
    self.value = value

#
# IDLNode
#
# A node in the AST.  The Node is composed of children, implemented as a
# dictionary of lists of child types and a dictionary of local properties,
# a.k.a. ExtendedAttributes.
#
class IDLNode(object):
  def __init__(self, cls, name, filename, lineno, pos, children):
    self.cls = cls
    self.name = name
    self.lineno = lineno
    self.pos = pos
    self.children = {}
    self.properties = { 'NAME' : name }
    self.hash = None
    self.typeref = None
    self.parent = None
    if children:
      for child in children:
        #
        # Copy children into local dictionary such that children of the
        # same class are added in order to the per key list.  All
        # ExtAttributes are filtered and applied to a property dictionary
        # instead of becoming children of IDLNode
        #
        if child.cls == 'ExtAttribute':
          self.properties[child.name] =  child.value
        else:
          if child.cls in self.children:
            self.children[child.cls].append(child)
          else:
            self.children[child.cls] = [child]

  # Return a string representation of this node
  def __str__(self):
    return "%s(%s)" % (self.cls, self.name)

  # Log an error for this object
  def Error(self, msg):
    ErrOut.LogLine(self.filename, self.lineno, 0, " %s %s" %
                   (self.FullName(), msg))

  # Log a warning for this object
  def Warning(self, msg):
    WarnOut.LogLine(self.filename, self.lineno, 0, " %s %s" %
                    (self.FullName(), msg))

  # Get a list of objects for this key
  def GetListOf(self, key):
    return self.children.get(key, [])

  # Get a list of all objects
  def Children(self):
    out = []
    for key in sorted(self.children.keys()):
      out.extend(self.children[key])
    return out

  # Dump this object and its children
  def Dump(self, depth, comments = False, out=sys.stdout):
    if self.cls == 'Comment' or self.cls == 'Copyright':
      is_comment = True
    else:
      is_comment = False

    # Skip this node if it's a comment, and we are not printing comments
    if not comments and is_comment: return

    tab = ""
    for t in range(depth):
      tab += '    '

    if is_comment:
      for line in self.name.split('\n'):
        out.write("%s%s\n" % (tab, line))
    else:
      out.write("%s%s\n" % (tab, self))

    if self.properties:
      out.write("%s  Properties\n" % tab)
      for p in self.properties:
        out.write("%s    %s : %s\n" % (tab, p, self.properties[p]))

    for cls in sorted(self.children.keys()):
      # Skip comments
      if (cls == 'Comment' or cls == 'Copyright') and not comments: continue

      out.write("%s  %ss\n" % (tab, cls))
      for c in self.children[cls]:
        c.Dump(depth + 1, comments = comments, out=out)

