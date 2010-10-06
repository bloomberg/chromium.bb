#!/usr/bin/env python
# Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for ExpectationsLine."""

import os
import sys
import unittest

from update_expectations_from_dashboard import ExpectationsLine


class ExpectationsLineUnittest(unittest.TestCase):
    ###########################################################################
    # Tests
    def setUp(self):
       pass

    def test___str___works(self):
        input = "BUG1 WIN RELEASE : test.html = FAIL"
        line = ExpectationsLine(input)
        self.assertEquals(input, str(line))

    def test_mismatched_attributes_cant_merge(self):
        line1 = ExpectationsLine("BUG1 SLOW WIN RELEASE : test.html = FAIL")
        line2 = ExpectationsLine("BUG1 WIN DEBUG : test.html = FAIL")

        self.assertFalse(line1.can_merge(line2))

    def test_mismatched_expectations_cant_merge(self):
        line1 = ExpectationsLine("BUG1 WIN RELEASE : test.html = TEXT")
        line2 = ExpectationsLine("BUG1 WIN DEBUG : test.html = IMAGE")

        self.assertFalse(line1.can_merge(line2))

    def test_mismatched_test_names_cant_merge(self):
        line1 = ExpectationsLine("BUG1 WIN RELEASE : test1.html = TEXT")
        line2 = ExpectationsLine("BUG1 WIN DEBUG : test2.html = TEXT")

        self.assertFalse(line1.can_merge(line2))


    def test_targets(self):
        input = ExpectationsLine("BUG13907 LINUX MAC : media/video-zoom.html = IMAGE PASS")

        self.assertTrue(('linux', 'release') in input.targets())

    def test_merges_if_build_is_covered(self):
        line1 = ExpectationsLine("BUG1 WIN RELEASE : test.html = FAIL")
        line2 = ExpectationsLine("BUG1 WIN DEBUG : test.html = FAIL")

        self.assertTrue(line1.can_merge(line2))
        line1.merge(line2)
        self.assertEquals("BUG1 WIN : test.html = FAIL", str(line1))

if '__main__' == __name__:
    unittest.main()
