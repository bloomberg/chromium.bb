#!/usr/bin/python
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

"""Compares the packages between 2 images by parsing the license file output."""

import re

from chromite.lib import commandline


def GetTreePackages(html_file):
  """Get the list of debian packages in an unpacked ProdNG tree.

  Args:
    html_file: which html license file to scan for packages.

  Returns:
    dictionary of packages and version numbers.
  """

  packages = {}

  # Grep and turn
  # <span class="title">ath6k-34</span>
  # into
  # ath6k 34
  exp = re.compile(r'<span class="title">(.+)-(.+)</span>')
  with open(html_file, 'r') as f:
    for line in f:
      match = exp.search(line)
      if match:
        packages[match.group(1)] = match.group(2)

  return packages


def ComparePkgLists(pkg_list1, pkg_list2):
  """Compare the package list in 2 dictionaries and output the differences.

  Args:
    pkg_list1: dict from GetTreePackages.
    pkg_list2: dict from GetTreePackages.

  Returns:
    N/A (outputs result on stdout).
  """

  for removed_package in sorted(set(pkg_list1) - set(pkg_list2)):
    print 'Package removed: %s-%s' % (
        removed_package, pkg_list1[removed_package])

  print
  for added_package in sorted(set(pkg_list2) - set(pkg_list1)):
    print 'Package added: %s-%s' % (
        added_package, pkg_list2[added_package])

  print
  for changed_package in sorted(set(pkg_list1) & set(pkg_list2)):
    ver1 = pkg_list1[changed_package]
    ver2 = pkg_list2[changed_package]
    if ver1 != ver2:
      print 'Package updated: %s from %s to %s' % (changed_package, ver1, ver2)


def main(args):
  parser = commandline.ArgumentParser(usage=__doc__)
  parser.add_argument('html1', metavar='license1.html', type='path',
                      help='old html file')
  parser.add_argument('html2', metavar='license2.html', type='path',
                      help='new html file')
  opts = parser.parse_args(args)

  pkg_list1 = GetTreePackages(opts.html1)
  pkg_list2 = GetTreePackages(opts.html2)
  ComparePkgLists(pkg_list1, pkg_list2)
