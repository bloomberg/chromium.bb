# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

from core import merge_script
from core import path_util


class TestMergeScript(unittest.TestCase):

  def testMergedJson(self):
    self.assertEqual(0, merge_script.MergeJson('output.json',
        [os.path.join(path_util.GetChromiumSrcDir(),
            'tools', 'perf', 'core', 'test_data', 'merge_1.json'),
        os.path.join(path_util.GetChromiumSrcDir(),
            'tools', 'perf', 'core', 'test_data', 'merge_2.json')],
        print_input=False))
    os.remove('output.json')
