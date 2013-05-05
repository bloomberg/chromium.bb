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

    def test_basic(self):
        expectations = bot_test_expectations.BotTestExpectations(only_ignore_very_flaky=True)
        test_data = {
            'tests': {
                'foo': {
                    'veryflaky.html': {
                        'results': [[1, 'F'], [1, 'P'], [1, 'F'], [1, 'P']]
                    },
                    'maybeflaky.html': {
                        'results': [[3, 'P'], [1, 'F'], [3, 'P']]
                    },
                    'notflakypass.html': {
                        'results': [[4, 'P']]
                    },
                    'notflakyfail.html': {
                        'results': [[4, 'F']]
                    },
                }
            }
        }
        output = expectations._generate_expectations_string(test_data)
        expected_output = """Bug(auto) foo/veryflaky.html [ Failure Pass ]"""
        self.assertMultiLineEqual(output, expected_output)

        expectations = bot_test_expectations.BotTestExpectations(only_ignore_very_flaky=False)
        output = expectations._generate_expectations_string(test_data)
        expected_output = """Bug(auto) foo/veryflaky.html [ Failure Pass ]
Bug(auto) foo/maybeflaky.html [ Failure Pass ]"""
        self.assertMultiLineEqual(output, expected_output)

    def test_all_failure_types(self):
        expectations = bot_test_expectations.BotTestExpectations(only_ignore_very_flaky=True)
        test_data = {
            'tests': {
                'foo': {
                    'allfailures.html': {
                        'results': [[1, 'F'], [1, 'P'], [1, 'F'], [1, 'P'],
                            [1, 'C'], [1, 'N'], [1, 'C'], [1, 'N'],
                            [1, 'T'], [1, 'X'], [1, 'T'], [1, 'X'],
                            [1, 'I'], [1, 'Z'], [1, 'I'], [1, 'Z'],
                            [1, 'O'], [1, 'C'], [1, 'O'], [1, 'C']]
                    },
                    'imageplustextflake.html': {
                        'results': [[1, 'Z'], [1, 'P'], [1, 'Z'], [1, 'P']]
                    },
                }
            }
        }
        output = expectations._generate_expectations_string(test_data)
        expected_output = """Bug(auto) foo/imageplustextflake.html [ Failure Pass ]
Bug(auto) foo/allfailures.html [ Crash Missing ImageOnlyFailure Failure Timeout Pass ]"""
        self.assertMultiLineEqual(output, expected_output)
