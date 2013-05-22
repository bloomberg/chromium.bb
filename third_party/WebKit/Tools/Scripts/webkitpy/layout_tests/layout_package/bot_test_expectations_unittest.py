# Copyright (C) 2013 Google Inc. All rights reserved.
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
#     * Neither the Google name nor the names of its
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

from webkitpy.layout_tests.layout_package import bot_test_expectations
from webkitpy.layout_tests.models import test_expectations


class BotTestExpectationsTest(unittest.TestCase):
    # FIXME: Find a way to import this map from Tools/TestResultServer/model/jsonresults.py.
    FAILURE_MAP = {"A": "AUDIO", "C": "CRASH", "F": "TEXT", "I": "IMAGE", "O": "MISSING",
        "N": "NO DATA", "P": "PASS", "T": "TIMEOUT", "Y": "NOTRUN", "X": "SKIP", "Z": "IMAGE+TEXT"}

    # All result_string's in this file expect newest result
    # on left: "PFF", means it just passed after 2 failures.

    def _assert_is_flaky(self, results_string, should_be_flaky):
        expectations = bot_test_expectations.BotTestExpectations(only_ignore_very_flaky=True)
        expectations._failure_map = self.FAILURE_MAP
        length_encoded = self._results_from_string(results_string)['results']
        num_actual_results = len(expectations._actual_results_for_test(length_encoded))
        if should_be_flaky:
            self.assertGreater(num_actual_results, 1)
        else:
            self.assertEqual(num_actual_results, 1)

    def test_basic_flaky(self):
        self._assert_is_flaky('PFF', False)  # Used to fail, but now passes.
        self._assert_is_flaky('FFP', False)  # Just started failing.
        self._assert_is_flaky('PFPF', True)  # Seen both failures and passes.
        # self._assert_is_flaky('PPPF', True)  # Should be counted as flaky but isn't yet.
        self._assert_is_flaky('FPPP', False)  # Just started failing, not flaky.
        self._assert_is_flaky('PFFP', True)  # Failed twice in a row, still flaky.
        # Failing 3+ times in a row is unlikely to be flaky, but rather a transient failure on trunk.
        # self._assert_is_flaky('PFFFP', False)
        # self._assert_is_flaky('PFFFFP', False)

    def _results_from_string(self, results_string):
        results_list = []
        last_char = None
        for char in results_string:
            if char != last_char:
                results_list.insert(0, [1, char])
            else:
                results_list[0][0] += 1
        return {'results': results_list}

    def _assert_expectations(self, expectations, test_data, expectations_string):
        output = expectations._generate_expectations(test_data, self.FAILURE_MAP)
        self.assertEqual(output, expectations_string)

    def test_basic(self):
        test_data = {
            'tests': {
                'foo': {
                    'veryflaky.html': self._results_from_string('FPFP'),
                    'maybeflaky.html': self._results_from_string('PPFP'),
                    'notflakypass.html': self._results_from_string('PPPP'),
                    'notflakyfail.html': self._results_from_string('FFFF'),
                }
            }
        }
        expectations = bot_test_expectations.BotTestExpectations(only_ignore_very_flaky=True)
        self._assert_expectations(expectations, test_data, {
            'foo/veryflaky.html': sorted(["TEXT", "PASS"]),
        })

        expectations = bot_test_expectations.BotTestExpectations(only_ignore_very_flaky=False)
        self._assert_expectations(expectations, test_data, {
            'foo/veryflaky.html': sorted(["TEXT", "PASS"]),
            'foo/maybeflaky.html': sorted(["TEXT", "PASS"]),
        })

    def test_all_failure_types(self):
        expectations = bot_test_expectations.BotTestExpectations(only_ignore_very_flaky=True)
        test_data = {
            'tests': {
                'foo': {
                    'allfailures.html': self._results_from_string('FPFPCNCNTXTXIZIZOCYOCY'),
                    'imageplustextflake.html': self._results_from_string('ZPZPPPPPPPPPPPPPPPPP'),
                }
            }
        }
        self._assert_expectations(expectations, test_data, {
            'foo/imageplustextflake.html': sorted(["IMAGE+TEXT", "PASS"]),
            'foo/allfailures.html': sorted(["TEXT", "PASS", "IMAGE+TEXT", "TIMEOUT", "CRASH", "IMAGE", "MISSING"]),
        })
