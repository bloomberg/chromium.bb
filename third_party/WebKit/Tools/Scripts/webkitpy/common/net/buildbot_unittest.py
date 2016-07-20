# Copyright (C) 2009 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest

from webkitpy.common.net.buildbot import BuildBot
from webkitpy.common.net.layouttestresults import LayoutTestResults


class BuilderTest(unittest.TestCase):

    def setUp(self):
        self.buildbot = BuildBot()

    def test_latest_layout_test_results(self):
        builder = BuildBot().builder_with_name('WebKit Mac10.8 (dbg)')
        builder.fetch_layout_test_results = lambda _: LayoutTestResults(None)
        self.assertTrue(builder.latest_layout_test_results())

    def test_results_url(self):
        builder = BuildBot().builder_with_name('WebKit Mac10.8 (dbg)')
        self.assertEqual(
            builder.results_url(),
            'https://storage.googleapis.com/chromium-layout-test-archives/WebKit_Mac10_8__dbg_')

    def test_accumulated_results_url(self):
        builder = BuildBot().builder_with_name('WebKit Mac10.8 (dbg)')
        self.assertEqual(
            builder.latest_layout_test_results_url(),
            'https://storage.googleapis.com/chromium-layout-test-archives/WebKit_Mac10_8__dbg_/results/layout-test-results')


class BuildBotTest(unittest.TestCase):

    def test_builder_with_name(self):
        buildbot = BuildBot()

        builder = buildbot.builder_with_name('Test Builder')
        self.assertEqual(builder.name(), 'Test Builder')
        self.assertEqual(builder.results_url(), 'https://storage.googleapis.com/chromium-layout-test-archives/Test_Builder')

        build = builder.build(10)
        self.assertEqual(build.builder(), builder)
        self.assertEqual(
            build.results_url(),
            'https://storage.googleapis.com/chromium-layout-test-archives/Test_Builder/10/layout-test-results')
