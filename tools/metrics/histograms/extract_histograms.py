# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Extract histogram names from the description XML file.

For more information on the format of the XML file, which is self-documenting,
see histograms.xml; however, here is a simple example to get you started. The
XML below will generate the following five histograms:

    HistogramTime
    HistogramEnum
    HistogramEnum_Chrome
    HistogramEnum_IE
    HistogramEnum_Firefox

<histogram-configuration>

<histograms>

<histogram name="HistogramTime" units="milliseconds">
  <summary>A brief description.</summary>
  <details>This is a more thorough description of this histogram.</details>
</histogram>

<histogram name="HistogramEnum" enum="MyEnumType">
  <summary>This histogram sports an enum value type.</summary>
</histogram>

</histograms>

<enums>

<enum name="MyEnumType">
  <summary>This is an example enum type, where the values mean little.</summary>
  <int value="1" label="FIRST_VALUE">This is the first value.</int>
  <int value="2" label="SECOND_VALUE">This is the second value.</int>
</enum>

</enums>

<fieldtrials>

<fieldtrial name="BrowserType">
  <group name="Chrome"/>
  <group name="IE"/>
  <group name="Firefox"/>
  <affected-histogram name="HistogramEnum"/>
</fieldtrial>

</fieldtrials>

</histogram-configuration>

"""

import copy
import logging
import xml.dom.minidom


MAX_FIELDTRIAL_DEPENDENCY_DEPTH = 5


class Error(Exception):
  pass


def _JoinChildNodes(tag):
  """Join child nodes into a single text.

  Applicable to leafs like 'summary' and 'detail'.

  Args:
    tag: parent node

  Returns:
    a string with concatenated nodes' text representation.
  """
  return ''.join(c.toxml() for c in tag.childNodes).strip()


def _NormalizeString(s):
  """Normalizes a string (possibly of multiple lines) by replacing each
  whitespace sequence with a single space.

  Args:
    s: The string to normalize, e.g. '  \n a  b c\n d  '

  Returns:
    The normalized string, e.g. 'a b c d'
  """
  return ' '.join(s.split())


def _NormalizeAllAttributeValues(node):
  """Recursively normalizes all tag attribute values in the given tree.

  Args:
    node: The minidom node to be normalized.

  Returns:
    The normalized minidom node.
  """
  if node.nodeType == xml.dom.minidom.Node.ELEMENT_NODE:
    for a in node.attributes.keys():
      node.attributes[a].value = _NormalizeString(node.attributes[a].value)

  for c in node.childNodes: _NormalizeAllAttributeValues(c)
  return node


def _ExpandHistogramNameWithFieldTrial(group_name, histogram_name, fieldtrial):
  """Creates a new histogram name based on the field trial group.

  Args:
    group_name: The name of the field trial group. May be empty.
    histogram_name: The name of the histogram. May be of the form
      Group.BaseName or BaseName
    field_trial: The FieldTrial XML element.

  Returns:
    A string with the expanded histogram name.

  Raises:
    Error if the expansion can't be done.
  """
  if fieldtrial.hasAttribute('separator'):
    separator = fieldtrial.getAttribute('separator')
  else:
    separator = '_'

  if fieldtrial.hasAttribute('ordering'):
    ordering = fieldtrial.getAttribute('ordering')
  else:
    ordering = 'suffix'
  if ordering not in ['prefix', 'suffix']:
    logging.error('ordering needs to be prefix or suffix, value is %s' %
                  ordering)
    raise Error()

  if not group_name:
    return histogram_name

  if ordering == 'suffix':
    return histogram_name + separator + group_name

  # For prefixes, the group_name is inserted between the "cluster" and the
  # "remainder", e.g. Foo.BarHist expanded with gamma becomes Foo.gamma_BarHist.
  sections = histogram_name.split('.')
  if len(sections) <= 1:
    logging.error(
      'Prefix Field Trial expansions require histogram names which include a '
      'dot separator. Histogram name is %s, and Field Trial is %s' %
      (histogram_name, fieldtrial.getAttribute('name')))
    raise Error()

  cluster = sections[0] + '.'
  remainder = '.'.join(sections[1:])
  return cluster + group_name + separator + remainder


def _ExtractEnumsFromXmlTree(tree):
  """Extract all <enum> nodes in the tree into a dictionary."""

  enums = {}
  have_errors = False

  last_name = None
  for enum in tree.getElementsByTagName("enum"):
    if enum.getAttribute('type') != 'int':
      logging.error('Unknown enum type %s' % enum.getAttribute('type'))
      have_errors = True
      continue

    name = enum.getAttribute('name')
    if last_name is not None and name.lower() < last_name.lower():
      logging.error('Enums %s and %s are not in alphabetical order'
                    % (last_name, name))
      have_errors = True
    last_name = name

    if name in enums:
      logging.error('Duplicate enum %s' % name)
      have_errors = True
      continue

    last_int_value = None
    enum_dict = {}
    enum_dict['name'] = name
    enum_dict['values'] = {}

    for int_tag in enum.getElementsByTagName("int"):
      value_dict = {}
      int_value = int(int_tag.getAttribute('value'))
      if last_int_value is not None and int_value < last_int_value:
        logging.error('Enum %s int values %d and %d are not in numerical order'
                      % (name, last_int_value, int_value))
        have_errors = True
      last_int_value = int_value
      if int_value in enum_dict['values']:
        logging.error('Duplicate enum value %d for enum %s' % (int_value, name))
        have_errors = True
        continue
      value_dict['label'] = int_tag.getAttribute('label')
      value_dict['summary'] = _JoinChildNodes(int_tag)
      enum_dict['values'][int_value] = value_dict

    summary_nodes = enum.getElementsByTagName("summary")
    if len(summary_nodes) > 0:
      enum_dict['summary'] = _NormalizeString(_JoinChildNodes(summary_nodes[0]))

    enums[name] = enum_dict

  return enums, have_errors


def _ExtractHistogramsFromXmlTree(tree, enums):
  """Extract all <histogram> nodes in the tree into a dictionary."""

  # Process the histograms. The descriptions can include HTML tags.
  histograms = {}
  have_errors = False
  last_name = None
  for histogram in tree.getElementsByTagName("histogram"):
    name = histogram.getAttribute('name')
    if last_name is not None and name.lower() < last_name.lower():
      logging.error('Histograms %s and %s are not in alphabetical order'
                    % (last_name, name))
      have_errors = True
    last_name = name
    if name in histograms:
      logging.error('Duplicate histogram definition %s' % name)
      have_errors = True
      continue
    histograms[name] = histogram_entry = {}

    # Find <summary> tag.
    summary_nodes = histogram.getElementsByTagName("summary")
    if len(summary_nodes) > 0:
      histogram_entry['summary'] = _NormalizeString(
          _JoinChildNodes(summary_nodes[0]))
    else:
      histogram_entry['summary'] = 'TBD'

    # Find <obsolete> tag.
    obsolete_nodes = histogram.getElementsByTagName("obsolete")
    if len(obsolete_nodes) > 0:
      reason = _JoinChildNodes(obsolete_nodes[0])
      histogram_entry['obsolete'] = reason

    # Handle units.
    if histogram.hasAttribute('units'):
      histogram_entry['units'] = histogram.getAttribute('units')

    # Find <details> tag.
    details_nodes = histogram.getElementsByTagName("details")
    if len(details_nodes) > 0:
      histogram_entry['details'] = _NormalizeString(
          _JoinChildNodes(details_nodes[0]))

    # Handle enum types.
    if histogram.hasAttribute('enum'):
      enum_name = histogram.getAttribute('enum')
      if not enum_name in enums:
        logging.error('Unknown enum %s in histogram %s' % (enum_name, name))
        have_errors = True
      else:
        histogram_entry['enum'] = enums[enum_name]

  return histograms, have_errors


def _UpdateHistogramsWithFieldTrialInformation(tree, histograms):
  """Process field trials' tags and combine with affected histograms.

  The histograms dictionary will be updated in-place by adding new histograms
  created by combining histograms themselves with field trials targetting these
  histograms.

  Args:
    tree: XML dom tree.
    histograms: a dictinary of histograms previously extracted from the tree;

  Returns:
    True if any errors were found.
  """
  have_errors = False

  # Verify order of fieldtrial fields first.
  last_name = None
  for fieldtrial in tree.getElementsByTagName("fieldtrial"):
    name = fieldtrial.getAttribute('name')
    if last_name is not None and name.lower() < last_name.lower():
      logging.error('Field trials %s and %s are not in alphabetical order'
                    % (last_name, name))
      have_errors = True
    last_name = name

  # Field trials can depend on other field trials, so we need to be careful.
  # Make a temporary copy of the list of field trials to use as a queue.
  # Field trials whose dependencies have not yet been processed will get
  # relegated to the back of the queue to be processed later.
  reprocess_queue = []
  def GenerateFieldTrials():
    for f in tree.getElementsByTagName("fieldtrial"): yield 0, f
    for r, f in reprocess_queue: yield r, f

  for reprocess_count, fieldtrial in GenerateFieldTrials():
    # Check dependencies first
    dependencies_valid = True
    affected_histograms = fieldtrial.getElementsByTagName('affected-histogram')
    for affected_histogram in affected_histograms:
      histogram_name = affected_histogram.getAttribute('name')
      if not histogram_name in histograms:
        # Base histogram is missing
        dependencies_valid = False
        missing_dependency = histogram_name
        break
    if not dependencies_valid:
      if reprocess_count < MAX_FIELDTRIAL_DEPENDENCY_DEPTH:
        reprocess_queue.append( (reprocess_count + 1, fieldtrial) )
        continue
      else:
        logging.error('Field trial %s is missing its dependency %s'
                      % (fieldtrial.getAttribute('name'),
                         missing_dependency))
        have_errors = True
        continue

    name = fieldtrial.getAttribute('name')
    groups = fieldtrial.getElementsByTagName('group')
    group_labels = {}
    for group in groups:
      group_labels[group.getAttribute('name')] = group.getAttribute('label')

    last_histogram_name = None
    for affected_histogram in affected_histograms:
      histogram_name = affected_histogram.getAttribute('name')
      if (last_histogram_name is not None
          and histogram_name.lower() < last_histogram_name.lower()):
        logging.error('Affected histograms %s and %s of field trial %s are not '
                      'in alphabetical order'
                      % (last_histogram_name, histogram_name, name))
        have_errors = True
      last_histogram_name = histogram_name
      base_description = histograms[histogram_name]
      with_groups = affected_histogram.getElementsByTagName('with-group')
      if len(with_groups) > 0:
        histogram_groups = with_groups
      else:
        histogram_groups = groups
      for group in histogram_groups:
        group_name = group.getAttribute('name')
        try:
          new_histogram_name = _ExpandHistogramNameWithFieldTrial(
            group_name, histogram_name, fieldtrial)
          if new_histogram_name != histogram_name:
            histograms[new_histogram_name] = copy.deepcopy(
              histograms[histogram_name])

          group_label = group_labels.get(group_name, '')

          if not 'fieldtrial_groups' in histograms[new_histogram_name]:
            histograms[new_histogram_name]['fieldtrial_groups'] = []
          histograms[new_histogram_name]['fieldtrial_groups'].append(group_name)

          if not 'fieldtrial_names' in histograms[new_histogram_name]:
            histograms[new_histogram_name]['fieldtrial_names'] = []
          histograms[new_histogram_name]['fieldtrial_names'].append(name)

          if not 'fieldtrial_labels' in histograms[new_histogram_name]:
            histograms[new_histogram_name]['fieldtrial_labels'] = []
          histograms[new_histogram_name]['fieldtrial_labels'].append(
            group_label)

        except Error:
          have_errors = True

  return have_errors


def ExtractHistogramsFromFile(file_handle):
  """Compute the histogram names and descriptions from the XML representation.

  Args:
    file_handle: A file or file-like with XML content.

  Returns:
    a tuple of (histograms, status) where histograms is a dictionary mapping
    histogram names to dictionaries containing histogram descriptions and status
    is a boolean indicating if errros were encoutered in processing.
  """
  tree = xml.dom.minidom.parse(file_handle)
  _NormalizeAllAttributeValues(tree)

  enums, enum_errors = _ExtractEnumsFromXmlTree(tree)
  histograms, histogram_errors = _ExtractHistogramsFromXmlTree(tree, enums)
  update_errors = _UpdateHistogramsWithFieldTrialInformation(tree, histograms)

  return histograms, enum_errors or histogram_errors or update_errors


def ExtractHistograms(filename):
  """Load histogram definitions from a disk file.
  Args:
    filename: a file path to load data from.

  Raises:
    Error if the file is not well-formatted.
  """
  with open(filename, 'r') as f:
    histograms, had_errors = ExtractHistogramsFromFile(f)
    if had_errors:
      logging.error('Error parsing %s' % filename)
      raise Error()
    return histograms


def ExtractNames(histograms):
  return sorted(histograms.keys())
