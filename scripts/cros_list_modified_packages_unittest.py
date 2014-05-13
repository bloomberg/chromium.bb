#!/usr/bin/python
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the cros_list_modified_packages program"""

import functools
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.lib import cros_test_lib
from chromite.scripts import cros_list_modified_packages


class ListModifiedWorkonPackagesTest(cros_test_lib.MockTestCase):
  """Test for cros_list_modified_packages.ListModifiedWorkonPackages."""

  def testListModifiedWorkonPackages(self):
    """Test that no ebuild breaks cros_list_modified_packages"""

    # A hook to set the "all_opt" parameter when calling ListWorkonPackages
    _ListWorkonPackagesPatch = \
      functools.partial(cros_list_modified_packages.ListWorkonPackages,
                        all_opt=True)

    with self.PatchObject(cros_list_modified_packages, 'ListWorkonPackages',
                          side_effect=_ListWorkonPackagesPatch):
      # ListModifiedWorkonPackages returns a generator object and doesn't
      # actually do any work automatically. We have to extract the elements
      # from it to get it to exercise the code, and we can do that by turning
      # it into a list.
      list(cros_list_modified_packages.ListModifiedWorkonPackages(None, True))


if __name__ == '__main__':
  cros_test_lib.main()
