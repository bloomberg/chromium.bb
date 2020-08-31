# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test gconv_strip."""

from __future__ import print_function

import os
import sys

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.scripts import gconv_strip

pytestmark = cros_test_lib.pytestmark_inside_only


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


class GconvStriptTest(cros_test_lib.MockTempDirTestCase):
  """Tests for gconv_strip script."""

  def testMultipleStringMatch(self):
    self.assertEqual(
        gconv_strip.MultipleStringMatch(
            [b'hell', b'a', b'z', b'k', b'spec'],
            b'hello_from a very special place'),
        [True, True, False, False, True])

  def testModuleRewrite(self):
    tmp_gconv_module = os.path.join(self.tempdir, 'gconv-modules')

    data = """
#      from          to            module         cost
alias  FOO           charset_foo
alias  BAR           charset_bar
module charset_foo   charset_bar   UNUSED_MODULE

#      from          to            module         cost
alias  CHAR_A        charset_A
alias  EUROPE        charset_B
module charset_A     charset_B     USED_MODULE
module charset_foo   charset_A     USED_MODULE
"""
    osutils.WriteFile(tmp_gconv_module, data)

    gmods = gconv_strip.GconvModules(tmp_gconv_module)
    self.assertEqual(gmods.Load(), [
        'BAR', 'CHAR_A', 'EUROPE', 'FOO', 'charset_A', 'charset_B',
        'charset_bar', 'charset_foo'])
    self.PatchObject(gconv_strip.lddtree, 'ParseELF', return_value={})
    class _StubStat(object):
      """Fake for lstat."""
      st_size = 0
    self.PatchObject(gconv_strip.os, 'lstat', return_value=_StubStat)
    self.PatchObject(gconv_strip.os, 'unlink')
    gmods.Rewrite(['charset_A', 'charset_B'], dry_run=False)

    expected = """
#      from          to            module         cost
alias  FOO           charset_foo

#      from          to            module         cost
alias  CHAR_A        charset_A
alias  EUROPE        charset_B
module charset_A     charset_B     USED_MODULE
module charset_foo   charset_A     USED_MODULE
"""

    content = osutils.ReadFile(tmp_gconv_module)
    self.assertEqual(content, expected)
