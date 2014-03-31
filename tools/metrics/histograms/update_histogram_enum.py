# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates enums in histograms.xml file with values read from provided C++ enum.

If the file was pretty-printed, the updated version is pretty-printed too.
"""

import logging
import os
import print_style
import re
import sys

from xml.dom import minidom

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
from diff_util import PromptUserToAcceptDiff

class UserError(Exception):
  def __init__(self, message):
    Exception.__init__(self, message)

  @property
  def message(self):
    return self.args[0]


def Log(message):
  logging.info(message)


def ExtractRegexGroup(line, regex):
  m = re.match(regex, line)
  if m:
    # TODO(ahernandez.miralles): This can throw an IndexError
    # if no groups are present; enclose in try catch block
    return m.group(1)
  else:
    return None


def ReadHistogramValues(filename, start_marker, end_marker):
  """Reads in values from |filename|, returning a list of (label, value) pairs
  corresponding to the enum framed by |start_marker| and |end_marker|.
  """
  # Read the file as a list of lines
  with open(filename) as f:
    content = f.readlines()

  # Locate the enum definition and collect all entries in it
  inside_enum = False # We haven't found the enum definition yet
  result = []
  for line in content:
    line = line.strip()
    if inside_enum:
      # Exit condition: we reached last enum value
      if re.match(end_marker, line):
        inside_enum = False
      else:
        # Inside enum: generate new xml entry
        label = ExtractRegexGroup(line.strip(), "^([\w]+)")
        if label:
          result.append((label, enum_value))
          enum_value += 1
    else:
      if re.match(start_marker, line):
        inside_enum = True
        enum_value = 0 # Start at 'UNKNOWN'
  return result


def UpdateHistogramDefinitions(histogram_enum_name, source_enum_values,
                               source_enum_path, document):
  """Sets the children of <enum name=|histogram_enum_name| ...> node in
  |document| to values generated from (label, value) pairs contained in
  |source_enum_values|.
  """
  # Find ExtensionFunctions enum.
  for enum_node in document.getElementsByTagName('enum'):
    if enum_node.attributes['name'].value == histogram_enum_name:
      histogram_enum_node = enum_node
      break
  else:
    raise UserError('No {0} enum node found'.format(histogram_enum_name))

  # Remove existing values.
  while histogram_enum_node.hasChildNodes():
    histogram_enum_node.removeChild(histogram_enum_node.lastChild)

  # Add a "Generated from (...)" comment
  comment = ' Generated from {0} '.format(source_enum_path)
  histogram_enum_node.appendChild(document.createComment(comment))

  # Add values generated from policy templates.
  for (label, value) in source_enum_values:
    node = document.createElement('int')
    node.attributes['value'] = str(value)
    node.attributes['label'] = label
    histogram_enum_node.appendChild(node)


def UpdateHistogramEnum(histogram_enum_name, source_enum_path,
                        start_marker, end_marker):
  """Updates |histogram_enum_name| enum in histograms.xml file with values
  read from |source_enum_path|, where |start_marker| and |end_marker| indicate
  the beginning and end of the source enum definition, respectively.
  """
  # TODO(ahernandez.miralles): The line below is present in nearly every
  # file in this directory; factor out into a central location
  HISTOGRAMS_PATH = 'histograms.xml'

  if len(sys.argv) > 1:
    print >>sys.stderr, 'No arguments expected!'
    sys.stderr.write(__doc__)
    sys.exit(1)

  Log('Reading histogram enum definition from "{0}".'.format(source_enum_path))
  source_enum_values = ReadHistogramValues(source_enum_path, start_marker,
                                           end_marker)

  Log('Reading existing histograms from "{0}".'.format(HISTOGRAMS_PATH))
  with open(HISTOGRAMS_PATH, 'rb') as f:
    histograms_doc = minidom.parse(f)
    f.seek(0)
    xml = f.read()

  Log('Comparing histograms enum with new enum definition.')
  UpdateHistogramDefinitions(histogram_enum_name, source_enum_values,
                             source_enum_path, histograms_doc)

  Log('Writing out new histograms file.')
  new_xml = print_style.GetPrintStyle().PrettyPrintNode(histograms_doc)
  if PromptUserToAcceptDiff(xml, new_xml, 'Is the updated version acceptable?'):
    with open(HISTOGRAMS_PATH, 'wb') as f:
      f.write(new_xml)

  Log('Done.')
