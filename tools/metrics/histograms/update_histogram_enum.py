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


def ReadHistogramValues(filename, start_marker, end_marker):
  """Reads in values from |filename|, returning a list of (label, value) pairs
  corresponding to the enum framed by |start_marker| and |end_marker|.
  """
  # Read the file as a list of lines
  with open(filename) as f:
    content = f.readlines()

  START_REGEX = re.compile(start_marker)
  ITEM_REGEX = re.compile(r'^(\w+)')
  ITEM_REGEX_WITH_INIT = re.compile(r'(\w+)\s*=\s*(\d+)')
  END_REGEX = re.compile(end_marker)

  # Locate the enum definition and collect all entries in it
  inside_enum = False # We haven't found the enum definition yet
  result = {}
  for line in content:
    line = line.strip()
    if inside_enum:
      # Exit condition: we reached last enum value
      if END_REGEX.match(line):
        inside_enum = False
      else:
        # Inside enum: generate new xml entry
        m = ITEM_REGEX_WITH_INIT.match(line)
        if m:
          enum_value = int(m.group(2))
          label = m.group(1)
        else:
          m = ITEM_REGEX.match(line)
          if m:
            label = m.group(1)
          else:
            continue
        result[enum_value] = label
        enum_value += 1
    else:
      if START_REGEX.match(line):
        inside_enum = True
        enum_value = 0
  return result


def CreateEnumItemNode(document, value, label):
  """Creates an int element to append to an enum."""
  item_node = document.createElement('int')
  item_node.attributes['value'] = str(value)
  item_node.attributes['label'] = label
  return item_node


def UpdateHistogramDefinitions(histogram_enum_name, source_enum_values,
                               source_enum_path, document):
  """Updates the enum node named |histogram_enum_name| based on the definition
  stored in |source_enum_values|. Existing items for which |source_enum_values|
  doesn't contain any corresponding data will be preserved. |source_enum_path|
  will be used to insert a comment.
  """
  # Get a dom of <enum name=|name| ...> node in |document|.
  for enum_node in document.getElementsByTagName('enum'):
    if enum_node.attributes['name'].value == histogram_enum_name:
      break
  else:
    raise UserError('No {0} enum node found'.format(name))

  new_children = []

  # Add a "Generated from (...)" comment.
  new_children.append(
      document.createComment(' Generated from {0} '.format(source_enum_path)))

  # Scan existing nodes in |enum_node| and build |new_children|.
  # - For each int node in |enum_node|, if there is a corresponding entry in
  #   |source_enum_values|, drop the existing node and add a node newly created
  #   from |source_enum_values| to |new_children|.
  # - Drop existing "Generated from (...)" comment in |enum_node|.
  # - Copy anything else.
  SOURCE_COMMENT_REGEX = re.compile('^ Generated from ')
  for child in enum_node.childNodes:
    if child.nodeName == 'int':
      value = int(child.attributes['value'].value)
      if source_enum_values.has_key(value):
        new_children.append(
            CreateEnumItemNode(document, value, source_enum_values[value]))
        del source_enum_values[value]
      else:
        new_children.append(child)
    # Drop existing source comments if any.
    elif (child.nodeType != minidom.Node.COMMENT_NODE or
          SOURCE_COMMENT_REGEX.match(child.data) is None):
      new_children.append(child)

  # Add remaining entries i.e. new enum values, in the |source_enum_values| to
  # the |new_children|.
  for value in sorted(source_enum_values.iterkeys()):
    new_children.append(
        CreateEnumItemNode(document, value, source_enum_values[value]))

  # Update |enum_node|.
  while enum_node.hasChildNodes():
    enum_node.removeChild(enum_node.lastChild)
  for child in new_children:
    enum_node.appendChild(child)


def UpdateHistogramFromDict(histogram_enum_name, source_enum_values,
                            source_enum_path):
  """Updates |histogram_enum_name| enum in histograms.xml file with values
  from the {value: 'key'} dictionary |source_enum_values|. A comment is added
  to histograms.xml citing that the values in |histogram_enum_name| were
  sourced from |source_enum_path|.
  """
  # TODO(ahernandez.miralles): The line below is present in nearly every
  # file in this directory; factor out into a central location
  HISTOGRAMS_PATH = 'histograms.xml'

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
  if not PromptUserToAcceptDiff(
      xml, new_xml, 'Is the updated version acceptable?'):
    Log('Cancelled.')
    return

  with open(HISTOGRAMS_PATH, 'wb') as f:
    f.write(new_xml)

  Log('Done.')


def UpdateHistogramEnum(histogram_enum_name, source_enum_path,
                        start_marker, end_marker):
  """Updates |histogram_enum_name| enum in histograms.xml file with values
  read from |source_enum_path|, where |start_marker| and |end_marker| indicate
  the beginning and end of the source enum definition, respectively.
  """

  Log('Reading histogram enum definition from "{0}".'.format(source_enum_path))
  source_enum_values = ReadHistogramValues(source_enum_path, start_marker,
                                           end_marker)

  UpdateHistogramFromDict(histogram_enum_name, source_enum_values,
      source_enum_path)