#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Pretty-prints the histograms.xml file, alphabetizing tags, wrapping text
at 80 chars, enforcing standard attribute ordering, and standardizing
indentation.

This is quite a bit more complicated than just calling tree.toprettyxml();
we need additional customization, like special attribute ordering in tags
and wrapping text nodes, so we implement our own full custom XML pretty-printer.
"""

from __future__ import with_statement

import logging
import os
import shutil
import sys
import xml.dom.minidom

import print_style

sys.path.insert(1, os.path.join(sys.path[0], '..', '..', 'python'))
from google import path_utils

# Import the metrics/common module.
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import diff_util

# Tags whose children we want to alphabetize. The key is the parent tag name,
# and the value is a pair of the tag name of the children we want to sort,
# and a key function that maps each child node to the desired sort key.
ALPHABETIZATION_RULES = {
  'histograms': ('histogram', lambda n: n.attributes['name'].value.lower()),
  'enums': ('enum', lambda n: n.attributes['name'].value.lower()),
  'enum': ('int', lambda n: int(n.attributes['value'].value)),
  'histogram_suffixes_list': (
      'histogram_suffixes', lambda n: n.attributes['name'].value.lower()),
  'histogram_suffixes': ('affected-histogram',
                         lambda n: n.attributes['name'].value.lower()),
  # TODO(yiyaoliu): Remove fieldtrial related pieces when it is not used.
  'fieldtrials': ('fieldtrial', lambda n: n.attributes['name'].value.lower()),
  'fieldtrial': ('affected-histogram',
                 lambda n: n.attributes['name'].value.lower()),
}


class Error(Exception):
  pass


def unsafeAppendChild(parent, child):
  """Append child to parent's list of children, ignoring the possibility that it
  is already in another node's childNodes list.  Requires that the previous
  parent of child is discarded (to avoid non-tree DOM graphs).
  This can provide a significant speedup as O(n^2) operations are removed (in
  particular, each child insertion avoids the need to traverse the old parent's
  entire list of children)."""
  child.parentNode = None
  parent.appendChild(child)
  child.parentNode = parent


def TransformByAlphabetizing(node):
  """Transform the given XML by alphabetizing specific node types according to
  the rules in ALPHABETIZATION_RULES.

  Args:
    node: The minidom node to transform.

  Returns:
    The minidom node, with children appropriately alphabetized. Note that the
    transformation is done in-place, i.e. the original minidom tree is modified
    directly.
  """
  if node.nodeType != xml.dom.minidom.Node.ELEMENT_NODE:
    for c in node.childNodes: TransformByAlphabetizing(c)
    return node

  # Element node with a tag name that we alphabetize the children of?
  if node.tagName in ALPHABETIZATION_RULES:
    # Put subnodes in a list of node,key pairs to allow for custom sorting.
    subtag, key_function = ALPHABETIZATION_RULES[node.tagName]
    subnodes = []
    last_key = -1
    for c in node.childNodes:
      if (c.nodeType == xml.dom.minidom.Node.ELEMENT_NODE and
          c.tagName == subtag):
        last_key = key_function(c)
      # Subnodes that we don't want to rearrange use the last node's key,
      # so they stay in the same relative position.
      subnodes.append( (c, last_key) )

    # Sort the subnode list.
    subnodes.sort(key=lambda pair: pair[1])

    # Re-add the subnodes, transforming each recursively.
    while node.firstChild:
      node.removeChild(node.firstChild)
    for (c, _) in subnodes:
      unsafeAppendChild(node, TransformByAlphabetizing(c))
    return node

  # Recursively handle other element nodes and other node types.
  for c in node.childNodes: TransformByAlphabetizing(c)
  return node


def PrettyPrint(raw_xml):
  """Pretty-print the given XML.

  Args:
    raw_xml: The contents of the histograms XML file, as a string.

  Returns:
    The pretty-printed version.
  """
  tree = xml.dom.minidom.parseString(raw_xml)
  tree = TransformByAlphabetizing(tree)
  return print_style.GetPrintStyle().PrettyPrintNode(tree)


def main():
  logging.basicConfig(level=logging.INFO)

  presubmit = ('--presubmit' in sys.argv)

  histograms_filename = 'histograms.xml'
  histograms_backup_filename = 'histograms.before.pretty-print.xml'

  # If there is a histograms.xml in the current working directory, use that.
  # Otherwise, use the one residing in the same directory as this script.
  histograms_dir = os.getcwd()
  if not os.path.isfile(os.path.join(histograms_dir, histograms_filename)):
    histograms_dir = path_utils.ScriptDir()

  histograms_pathname = os.path.join(histograms_dir, histograms_filename)
  histograms_backup_pathname = os.path.join(histograms_dir,
                                            histograms_backup_filename)

  logging.info('Loading %s...' % os.path.relpath(histograms_pathname))
  with open(histograms_pathname, 'rb') as f:
    xml = f.read()

  # Check there are no CR ('\r') characters in the file.
  if '\r' in xml:
    logging.info('DOS-style line endings (CR characters) detected - these are '
                 'not allowed. Please run dos2unix %s' % histograms_filename)
    sys.exit(1)

  logging.info('Pretty-printing...')
  try:
    pretty = PrettyPrint(xml)
  except Error:
    logging.error('Aborting parsing due to fatal errors.')
    sys.exit(1)

  if xml == pretty:
    logging.info('%s is correctly pretty-printed.' % histograms_filename)
    sys.exit(0)
  if presubmit:
    logging.info('%s is not formatted correctly; run pretty_print.py to fix.' %
                 histograms_filename)
    sys.exit(1)
  if not diff_util.PromptUserToAcceptDiff(
      xml, pretty,
      'Is the prettified version acceptable?'):
    logging.error('Aborting')
    return

  logging.info('Creating backup file %s' % histograms_backup_filename)
  shutil.move(histograms_pathname, histograms_backup_pathname)

  logging.info('Writing new %s file' % histograms_filename)
  with open(histograms_pathname, 'wb') as f:
    f.write(pretty)


if __name__ == '__main__':
  main()
