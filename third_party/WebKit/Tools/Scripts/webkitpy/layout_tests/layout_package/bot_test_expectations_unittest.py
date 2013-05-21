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


class BotTestExpectationsTest(unittest.TestCase):
    # Expects newest result on left of string "PFF", means it just passed after 2 failures.
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
        output = expectations._generate_expectations_string(test_data)
        self.assertMultiLineEqual(output, expectations_string)

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
        self._assert_expectations(expectations, test_data, """Bug(auto) foo/veryflaky.html [ Failure Pass ]""")

        expectations = bot_test_expectations.BotTestExpectations(only_ignore_very_flaky=False)
        self._assert_expectations(expectations, test_data, """Bug(auto) foo/veryflaky.html [ Failure Pass ]
Bug(auto) foo/maybeflaky.html [ Failure Pass ]""")

    def test_all_failure_types(self):
        expectations = bot_test_expectations.BotTestExpectations(only_ignore_very_flaky=True)
        test_data = {
            'tests': {
                'foo': {
                    'allfailures.html': self._results_from_string('FPFPCNCNTXTXIZIZOCOC'),
                    'imageplustextflake.html': self._results_from_string('ZPZPPPPPPPPPPPPPPPPP'),
                }
            }
        }
        self._assert_expectations(expectations, test_data, """Bug(auto) foo/imageplustextflake.html [ Failure Pass ]
Bug(auto) foo/allfailures.html [ Crash Missing ImageOnlyFailure Failure Timeout Pass ]""")
