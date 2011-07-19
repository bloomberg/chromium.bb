#!/usr/bin/python
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Visitor Object for traversing AST """

#
# IDLVisitor
#
# The IDLVisitor class will traverse an AST truncating portions of the tree
# that fail due to class or version filters.  For each node, after the filter
# passes, the visitor will call the 'Arive' member passing in the node and
# and data passing in from the parent call.   It will then Visit the children.
# When done processing children, the visitor will call the 'Depart' member
# before returning
#

class IDLVisitor(object):
  def __init__(self):
    self.depth = 0

  # Return TRUE if the node should be visited
  def VisitFilter(self, node, data):
    return True

  # Return TRUE if data should be added to the childdata list
  def AgrigateFilter(self, data):
    return data is not None

  def Visit(self, node, data):
    self.depth += 1
    if not self.VisitFilter(node, data): return None

    childdata = []
    newdata = self.Arrive(node, data)
    for child in node.GetChildren():
      ret = self.Visit(child, newdata)
      if self.AgrigateFilter(ret):
        childdata.append(ret)
    out = self.Depart(node, newdata, childdata)

    self.depth -= 1
    return out

  def Arrive(self, node, data):
    return data

  def Depart(self, node, data, childdata):
    return data


#
# IDLVersionVisitor
#
# The IDLVersionVisitor will only visit nodes with intervals that include the
# version.  It will also optionally filter based on a class list
#
class IDLVersionVisitor(object):
  def __init__(self, version, classList):
    self.version = version
    self.classes = classes

  def Filter(self, node, data):
    if self.classList and node.cls not in self.classList: return False
    if not node.IsVersion(self.version): return False
    return True

class IDLRangeVisitor(object):
  def __init__(self, vmin, vmax, classList):
    self.vmin = vmin
    self.vmax = vmax
    self.classList = classList

  def Filter(self, node, data):
    if self.classList and node.cls not in self.classList: return False
    if not node.IsVersion(self.version): return False
    return True


