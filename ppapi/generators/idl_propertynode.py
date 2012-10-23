#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Hierarchical property system for IDL AST """
import re
import sys

from idl_log import ErrOut, InfoOut, WarnOut
from idl_option import GetOption, Option, ParseOptions

#
# IDLPropertyNode
#
# A property node is a hierarchically aware system for mapping
# keys to values, such that a local dictionary is search first,
# followed by parent dictionaries in order.
#
class IDLPropertyNode(object):
  def __init__(self):
    self.parents = []
    self.property_map = {}

  def Error(self, msg):
    name = self.GetProperty('NAME', 'Unknown')
    parents = [parent.GetProperty('NAME', '???') for parent in self.parents]
    ErrOut.Log('%s [%s] : %s' % (name, ' '.join(parents), msg))

  def AddParent(self, parent):
    assert parent
    self.parents.append(parent)

  def SetProperty(self, name, val):
    self.property_map[name] = val

  def _GetProperty_(self, name):
    # Check locally for the property, and return it if found.
    prop = self.property_map.get(name, None)
    if prop is not None: return prop
    # If not, seach parents in order
    for parent in self.parents:
      prop = parent.GetProperty(name)
      if prop is not None: return prop
    # Otherwise, it can not be found.
    return None

  def GetProperty(self, name, default=None):
    prop = self._GetProperty_(name)
    if prop is None:
      return default
    else:
      return prop

  def GetPropertyLocal(self, name, default=None):
    # Search for the property, but only locally, returning the
    # default if not found.
    prop = self.property_map.get(name, default)
    return prop

  # Regular expression to parse property keys in a string such that a string
  #  "My string $NAME$" will find the key "NAME".
  regex_var = re.compile('(?P<src>[^\\$]+)|(?P<key>\\$\\w+\\$)')

  def GetPropertyList(self):
    return self.property_map.keys()

  # Recursively expands text keys in the form of $KEY$ with the value
  # of the property of the same name.  Since this is done recursively
  # one property can be defined in terms of another.
  def Replace(self, text):
    itr = IDLPropertyNode.regex_var.finditer(text)
    out = ''
    for m in itr:
      (start, stop) = m.span()
      if m.lastgroup == 'src':
        out += text[start:stop]
      if m.lastgroup == 'key':
        key = text[start+1:stop-1]
        val = self.GetProperty(key, None)
        if not val:
          self.Error('No property "%s"' % key)
        out += self.Replace(str(val))
    return out


#
# Testing functions
#

# Build a property node, setting the properties including a name, and
# associate the children with this new node.
#
def BuildNode(name, props, children=[], parents=[]):
  node = IDLPropertyNode()
  node.SetProperty('NAME', name)
  for prop in props:
    toks = prop.split('=')
    node.SetProperty(toks[0], toks[1])
  for child in children:
    child.AddParent(node)
  for parent in parents:
    node.AddParent(parent)
  return node

def ExpectProp(node, name, val):
  found = node.GetProperty(name)
  if found != val:
    ErrOut.Log('Got property %s expecting %s' % (found, val))
    return 1
  return 0

#
# Verify property inheritance
#
def PropertyTest():
  errors = 0
  left = BuildNode('Left', ['Left=Left'])
  right = BuildNode('Right', ['Right=Right'])
  top = BuildNode('Top', ['Left=Top', 'Right=Top'], [left, right])

  errors += ExpectProp(top, 'Left', 'Top')
  errors += ExpectProp(top, 'Right', 'Top')

  errors += ExpectProp(left, 'Left', 'Left')
  errors += ExpectProp(left, 'Right', 'Top')

  errors += ExpectProp(right, 'Left', 'Top')
  errors += ExpectProp(right, 'Right', 'Right')

  if not errors: InfoOut.Log('Passed PropertyTest')
  return errors


def ExpectText(node, text, val):
  found = node.Replace(text)
  if found != val:
    ErrOut.Log('Got replacement %s expecting %s' % (found, val))
    return 1
  return 0

#
# Verify text replacement
#
def ReplaceTest():
  errors = 0
  left = BuildNode('Left', ['Left=Left'])
  right = BuildNode('Right', ['Right=Right'])
  top = BuildNode('Top', ['Left=Top', 'Right=Top'], [left, right])

  errors += ExpectText(top, '$Left$', 'Top')
  errors += ExpectText(top, '$Right$', 'Top')

  errors += ExpectText(left, '$Left$', 'Left')
  errors += ExpectText(left, '$Right$', 'Top')

  errors += ExpectText(right, '$Left$', 'Top')
  errors += ExpectText(right, '$Right$', 'Right')

  if not errors: InfoOut.Log('Passed ReplaceTest')
  return errors


def MultiParentTest():
  errors = 0

  parent1 = BuildNode('parent1', ['PARENT1=parent1', 'TOPMOST=$TOP$'])
  parent2 = BuildNode('parent2', ['PARENT1=parent2', 'PARENT2=parent2'])
  child = BuildNode('child', ['CHILD=child'], parents=[parent1, parent2])
  BuildNode('top', ['TOP=top'], children=[parent1])

  errors += ExpectText(child, '$CHILD$', 'child')
  errors += ExpectText(child, '$PARENT1$', 'parent1')
  errors += ExpectText(child, '$PARENT2$', 'parent2')

  # Verify recursive resolution
  errors += ExpectText(child, '$TOPMOST$', 'top')

  if not errors: InfoOut.Log('Passed MultiParentTest')
  return errors


def Main():
  errors = 0
  errors += PropertyTest()
  errors += ReplaceTest()
  errors += MultiParentTest()

  if errors:
    ErrOut.Log('IDLNode failed with %d errors.' % errors)
    return  -1
  return 0


if __name__ == '__main__':
  sys.exit(Main())

