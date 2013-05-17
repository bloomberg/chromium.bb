# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
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

import unittest2 as unittest
import json
import optparse
import random

from webkitpy.common.host_mock import MockHost
from webkitpy.layout_tests.layout_package import json_results_generator
from webkitpy.layout_tests.models import test_expectations, test_results
from webkitpy.layout_tests.port import test
from webkitpy.thirdparty.mock import Mock


class JSONGeneratorTest(unittest.TestCase):
    def setUp(self):
        self.builder_name = 'DUMMY_BUILDER_NAME'
        self.build_name = 'DUMMY_BUILD_NAME'
        self.build_number = 'DUMMY_BUILDER_NUMBER'

        # For archived results.
        self._json = None
        self._num_runs = 0
        self._tests_set = set([])
        self._test_timings = {}
        self._failed_count_map = {}

        self._PASS_count = 0
        self._DISABLED_count = 0
        self._FLAKY_count = 0
        self._FAILS_count = 0
        self._fixable_count = 0

    def test_strip_json_wrapper(self):
        json = "['contents']"
        self.assertEqual(json_results_generator.strip_json_wrapper(json_results_generator._JSON_PREFIX + json + json_results_generator._JSON_SUFFIX), json)
        self.assertEqual(json_results_generator.strip_json_wrapper(json), json)

    def test_test_timings_trie(self):
        test_port = test.TestPort(MockHost())
        individual_test_timings = []
        individual_test_timings.append(test_results.TestResult('foo/bar/baz.html', test_run_time=1.2))
        individual_test_timings.append(test_results.TestResult('bar.html', test_run_time=0.0001))
        trie = json_results_generator.test_timings_trie(test_port, individual_test_timings)

        expected_trie = {
          'bar.html': 0,
          'foo': {
              'bar': {
                  'baz.html': 1200,
              }
          }
        }

        self.assertEqual(json.dumps(trie), json.dumps(expected_trie))
