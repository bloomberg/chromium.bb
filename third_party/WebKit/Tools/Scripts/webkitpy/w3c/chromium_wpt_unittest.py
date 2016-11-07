# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.w3c.chromium_wpt import ChromiumWPT


class ChromiumWPTTest(unittest.TestCase):

    def test_has_changes_in_wpt(self):
        host = MockHost()

        def run_command_fn(_):
            return ("something/something.html\n"
                    "third_party/WebKit/LayoutTests/imported/wpt/something.html\n")

        host.executive = MockExecutive2(run_command_fn=run_command_fn)
        chromium_wpt = ChromiumWPT(host)

        self.assertTrue(chromium_wpt.has_changes_in_wpt('sha'))

    def test_has_changes_in_wpt_looks_at_start_of_string(self):
        host = MockHost()

        def run_command_fn(_):
            return ("something/something.html\n"
                    "something/third_party/WebKit/LayoutTests/imported/wpt/something.html\n")

        host.executive = MockExecutive2(run_command_fn=run_command_fn)
        chromium_wpt = ChromiumWPT(host)

        self.assertFalse(chromium_wpt.has_changes_in_wpt('sha'))

    def test_has_changes_in_wpt_does_not_count_expectation_files(self):
        host = MockHost()

        def run_command_fn(_):
            return ("something/something.html\n"
                    "third_party/WebKit/LayoutTests/imported/wpt/something-expected.html\n"
                    "-expected.txt\n")

        host.executive = MockExecutive2(run_command_fn=run_command_fn)
        chromium_wpt = ChromiumWPT(host)

        self.assertFalse(chromium_wpt.has_changes_in_wpt('sha'))
