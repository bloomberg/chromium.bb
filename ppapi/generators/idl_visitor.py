# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Visitor Object for traversing AST """

#
# IDLVisitor
#
# The IDLVisitor class will traverse an AST truncating portions of the tree
# when 'VisitFilter' returns false.  After the filter returns true, for each
# node, the visitor will call the 'Arrive' member passing in the node and
# and generic data object from the parent call.  The returned value is then
# passed to all children who's results are aggregated into a list.  The child
# results along with the original Arrive result are passed to the Depart
# function which returns the final result of the Visit.  By default this is
# the exact value that was return from the original arrive.
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
    __pychecker__ = 'unusednames=node'
    return data

  def Depart(self, node, data, childdata):
    __pychecker__ = 'unusednames=node,childdata'
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
