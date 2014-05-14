#!/usr/bin/python
# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the cbuildbot program"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.cbuildbot import constants
from chromite.lib import cros_test_lib
from chromite.scripts import cbuildbot


class IsDistributedBuilderTest(cros_test_lib.TestCase):
  """Test for cbuildbot._IsDistributedBuilder."""

  # pylint: disable=W0212
  def testIsDistributedBuilder(self):
    """Tests for _IsDistributedBuilder() under various configurations."""
    parser = cbuildbot._CreateParser()
    argv = ['x86-generic-paladin']
    (options, _) = cbuildbot._ParseCommandLine(parser, argv)
    options.buildbot = False
    options.pre_cq = False

    build_config = dict(pre_cq=False,
                        manifest_version=False)
    chrome_rev = None

    def _TestConfig(expected):
      self.assertEquals(expected,
                        cbuildbot._IsDistributedBuilder(
                            options=options,
                            chrome_rev=chrome_rev,
                            build_config=build_config))

    # Default options.
    _TestConfig(False)

    # In Pre-CQ mode, we run as as a distributed builder.
    options.pre_cq = True
    _TestConfig(True)

    options.pre_cq = False
    build_config['pre_cq'] = True
    _TestConfig(True)

    build_config['pre_cq'] = False
    build_config['manifest_version'] = True
    # Not running in buildbot mode even though manifest_version=True.
    _TestConfig(False)
    options.buildbot = True
    _TestConfig(True)

    for chrome_rev in (constants.CHROME_REV_TOT,
                       constants.CHROME_REV_LOCAL,
                       constants.CHROME_REV_SPEC):
      _TestConfig(False)


if __name__ == '__main__':
  cros_test_lib.main()
