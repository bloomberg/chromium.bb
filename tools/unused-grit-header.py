#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to scan source files for unneeded grit includes."""

import os
import sys
import xml.etree.ElementTree

IF_ELSE_TAGS = ('if', 'else')


def Usage(prog_name):
  print prog_name, 'GRD_FILE DIRS_TO_SCAN'


def GetResourcesForNode(node, parent_file, resource_tag):
  """Recursively iterate through a node and extract resource names.

  Args:
    node: The node to iterate through.
    parent_file: The file that contains node.
    resource_tag: The resource tag to extract names from.

  Returns:
    A list of resource names.
  """
  resources = []
  for child in node.getchildren():
    if child.tag == resource_tag:
      resources.append(child.attrib['name'])
    elif child.tag in IF_ELSE_TAGS:
      resources.extend(GetResourcesForNode(child, parent_file, resource_tag))
    elif child.tag == 'part':
      parent_dir = os.path.dirname(parent_file)
      part_file = os.path.join(parent_dir, child.attrib['file'])
      part_tree = xml.etree.ElementTree.parse(part_file)
      part_root = part_tree.getroot()
      assert part_root.tag == 'grit-part'
      resources.extend(GetResourcesForNode(part_root, part_file, resource_tag))
    else:
      raise Exception('unknown tag:', child.tag)
  return resources


def FindNodeWithTag(node, tag):
  """Look through a node's children for a child node with a given tag.

  Args:
    root: The node to examine.
    tag: The tag on a child node to look for.

  Returns:
    A child node with the given tag, or None.
  """
  result = None
  for n in node.getchildren():
    if n.tag == tag:
      assert not result
      result = n
  return result


def GetResourcesForGrdFile(tree, grd_file):
  """Find all the message and include resources from a given grit file.

  Args:
    tree: The XML tree.
    grd_file: The file that contains the XML tree.

  Returns:
    A list of resource names.
  """
  root = tree.getroot()
  assert root.tag == 'grit'
  release_node = FindNodeWithTag(root, 'release')
  assert release_node != None

  messages_node = FindNodeWithTag(release_node, 'messages')
  messages = set()
  if messages_node != None:
    messages = set(GetResourcesForNode(messages_node, grd_file, 'message'))

  includes_node = FindNodeWithTag(release_node, 'includes')
  includes = set()
  if includes_node != None:
    includes = set(GetResourcesForNode(includes_node, grd_file, 'include'))
  return messages.union(includes)


def GetOutputFileForNode(node):
  """Find the output file starting from a given node.

  Args:
    node: The root node to scan from.

  Returns:
    A grit header file name.
  """
  output_file = None
  for child in node.getchildren():
    if child.tag == 'output':
      if child.attrib['type'] == 'rc_header':
        assert output_file is None
        output_file = child.attrib['filename']
    elif child.tag in IF_ELSE_TAGS:
      child_output_file = GetOutputFileForNode(child)
      if not child_output_file:
        continue
      assert output_file is None
      output_file = child_output_file
    else:
      raise Exception('unknown tag:', child.tag)
  return output_file


def GetOutputHeaderFile(tree):
  """Find the output file for a given tree.

  Args:
    tree: The tree to scan.

  Returns:
    A grit header file name.
  """
  root = tree.getroot()
  assert root.tag == 'grit'
  output_node = FindNodeWithTag(root, 'outputs')
  assert output_node != None
  return GetOutputFileForNode(output_node)


def ShouldScanFile(filename):
  """Return if the filename has one of the extensions below."""
  extensions = ['.cc', '.cpp', '.h', '.mm']
  file_extension = os.path.splitext(filename)[1]
  return file_extension in extensions


def NeedsGritInclude(grit_header, resources, filename):
  """Return whether a file needs a given grit header or not.

  Args:
    grit_header: The grit header file name.
    resources: The list of resource names in grit_header.
    filename: The file to scan.

  Returns:
    True if the file should include the grit header.
  """
  with open(filename, 'rb') as f:
    grit_header_line = grit_header + '"\n'
    has_grit_header = False
    while True:
      line = f.readline()
      if not line:
        break
      if line.endswith(grit_header_line):
        has_grit_header = True
        break

    if not has_grit_header:
      return True
    rest_of_the_file = f.read()
    return any(resource in rest_of_the_file for resource in resources)


def main(argv):
  if len(argv) < 3:
    Usage(argv[0])
    return 1
  grd_file = argv[1]
  dirs_to_scan = argv[2:]
  for f in dirs_to_scan:
    if not os.path.exists(f):
      print 'Error: %s does not exist' % f
      return 1

  tree = xml.etree.ElementTree.parse(grd_file)
  grit_header = GetOutputHeaderFile(tree)
  if not grit_header:
    print 'Error: %s does not generate any output headers.' % grit_header
    return 1
  resources = GetResourcesForGrdFile(tree, grd_file)

  files_with_unneeded_grit_includes = []
  for dir_to_scan in dirs_to_scan:
    for root, dirs, files in os.walk(dir_to_scan):
      if '.git' in dirs:
        dirs.remove('.git')
      full_paths = [os.path.join(root, f) for f in files if ShouldScanFile(f)]
      files_with_unneeded_grit_includes.extend(
          [f for f in full_paths
           if not NeedsGritInclude(grit_header, resources, f)])
  if files_with_unneeded_grit_includes:
    print '\n'.join(files_with_unneeded_grit_includes)
    return 2
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
