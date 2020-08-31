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

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import diff_util
import presubmit_util

import etree_util
import histograms_print_style

class Error(Exception):
  pass

UNIT_REWRITES = {
  'mcs': 'microseconds',
  'microsecond': 'microseconds',
  'us': 'microseconds',
  'millisecond': 'ms',
  'milliseconds': 'ms',
  'kb': 'KB',
  'kB': 'KB',
  'kilobytes': 'KB',
  'kbits/s': 'kbps',
  'mb': 'MB',
  'mB': 'MB',
  'megabytes': 'MB',
  'mbits/s': 'mbps',
  'percent': '%',
  'Percent': '%',
  'percentage': '%',
}

def canonicalizeUnits(tree):
  """Canonicalize the spelling of certain units in histograms."""
  if tree.tag == 'histogram':
    units = tree.get('units')
    if units and units in UNIT_REWRITES:
      tree.set('units', UNIT_REWRITES[units])

  for child in tree:
    canonicalizeUnits(child)

def fixObsoleteOrder(tree):
  """Put obsolete tags at the beginning of histogram tags."""
  obsoletes = []

  for child in tree:
    if child.tag == 'obsolete':
      obsoletes.append(child)
    else:
      fixObsoleteOrder(child)

  for obsolete in obsoletes:
    tree.remove(obsolete)

  # Only keep the first obsolete tag.
  if obsoletes:
    tree.insert(0, obsoletes[0])

def DropNodesByTagName(tree, tag, dropped_nodes=[]):
  """Drop all nodes with named tag from the XML tree."""
  removes = []

  for child in tree:
    if child.tag == tag:
      removes.append(child)
      dropped_nodes.append(child)
    else:
      DropNodesByTagName(child, tag)

  for child in removes:
    tree.remove(child)

def FixMisplacedHistogramsAndHistogramSuffixes(tree):
  """Fixes misplaced histogram and histogram_suffixes nodes."""
  histograms = []
  histogram_suffixes = []

  def ExtractMisplacedHistograms(tree):
    """Gets and drops misplaced histograms and histogram_suffixes.

    Args:
      tree: The node of the xml tree.
      histograms: A list of histogram nodes inside histogram_suffixes_list
          node. This is a return element.
      histogram_suffixes: A list of histogram_suffixes nodes inside histograms
          node. This is a return element.
    """
    for child in tree:
      if child.tag == 'histograms':
        DropNodesByTagName(child, 'histogram_suffixes', histogram_suffixes)
      elif child.tag == 'histogram_suffixes_list':
        DropNodesByTagName(child, 'histogram', histograms)
      else:
        ExtractMisplacedHistograms(child)

  ExtractMisplacedHistograms(tree)

  def AddBackMisplacedHistograms(tree):
    """Adds back those misplaced histogram and histogram_suffixes nodes."""
    for child in tree:
      if child.tag == 'histograms':
        child.extend(histograms)
      elif child.tag == 'histogram_suffixes_list':
        child.extend(histogram_suffixes)
      else:
        AddBackMisplacedHistograms(child)

  AddBackMisplacedHistograms(tree)

def PrettyPrintHistograms(raw_xml):
  """Pretty-print the given histograms XML.

  Args:
    raw_xml: The contents of the histograms XML file, as a string.

  Returns:
    The pretty-printed version.
  """
  top_level_content = etree_util.GetTopLevelContent(raw_xml)
  root = etree_util.ParseXMLString(raw_xml)
  return top_level_content + PrettyPrintHistogramsTree(root)

def PrettyPrintHistogramsTree(tree):
  """Pretty-print the given ElementTree element.

  Args:
    tree: The ElementTree element.

  Returns:
    The pretty-printed version as an XML string.
  """
  # Prevent accidentally adding enums to histograms.xml
  DropNodesByTagName(tree, 'enums')
  FixMisplacedHistogramsAndHistogramSuffixes(tree)
  canonicalizeUnits(tree)
  fixObsoleteOrder(tree)
  return histograms_print_style.GetPrintStyle().PrettyPrintXml(tree)

def PrettyPrintEnums(raw_xml):
  """Pretty print the given enums XML."""

  root = etree_util.ParseXMLString(raw_xml)

  # Prevent accidentally adding histograms to enums.xml
  DropNodesByTagName(root, 'histograms')
  DropNodesByTagName(root, 'histogram_suffixes_list')

  top_level_content = etree_util.GetTopLevelContent(raw_xml)

  formatted_xml = (histograms_print_style.GetPrintStyle()
                  .PrettyPrintXml(root))
  return top_level_content + formatted_xml

def main():
  status1 = presubmit_util.DoPresubmit(
      sys.argv, 'enums.xml', 'enums.before.pretty-print.xml', PrettyPrintEnums)
  status2 = presubmit_util.DoPresubmit(sys.argv, 'histograms.xml',
                                       'histograms.before.pretty-print.xml',
                                       PrettyPrintHistograms)
  sys.exit(status1 or status2)

if __name__ == '__main__':
  main()
