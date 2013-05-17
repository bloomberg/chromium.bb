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

"""Generates a fake TestExpectations file consisting of flaky tests from the bot
corresponding to the give port."""

import json
import logging
import os.path
import urllib
import urllib2

from webkitpy.layout_tests.port import builders

_log = logging.getLogger(__name__)


class BotTestExpectations(object):
    RLE_LENGTH = 0
    RLE_VALUE = 1
    RESULTS_KEY = 'results'
    TESTS_KEY = 'tests'
    RESULTS_URL_PREFIX = 'http://test-results.appspot.com/testfile?master=ChromiumWebkit&testtype=layout-tests&name=results-small.json&builder='

    # FIXME: This data is now in the results json. Get it from there.
    MAP_ENCODED_RESULT_STRING_TO_EXPECTATIONS_VALUE = {
        'P': 'Pass',
        'N': '', # No data
        'X': '', # Skip
        'T': 'Timeout',
        'F': 'Failure', # text-only
        'C': 'Crash',
        'I': 'ImageOnlyFailure',
        'Z': 'Failure', # image+text
        'O': 'Missing',
    };

    def __init__(self, only_ignore_very_flaky):
        self._only_ignore_very_flaky = only_ignore_very_flaky

    def expectations_string(self, port_name):
        builder_name = builders.builder_name_for_port_name(port_name)
        if not builder_name:
            return ""

        url = self.RESULTS_URL_PREFIX + urllib.quote(builder_name)
        try:
            _log.debug('Fetching flakiness data from appengine.')
            data = urllib2.urlopen(url)
            parsed_data = json.load(data)[builder_name]
            result = self._generate_expectations_string(parsed_data)
            return result
        except urllib2.URLError as error:
            _log.warning('Could not retrieve flakiness data from the bot.')
            _log.warning(error)
            return ""

    def _generate_expectations_string(self, test_data):
        out = []
        self._walk_tests_trie(test_data[self.TESTS_KEY], out)
        return "\n".join(out)

    def _actual_results_for_test(self, run_length_encoded_results):
        resultsMap = {}

        seenResults = {};
        for result in run_length_encoded_results:
            numResults = result[self.RLE_LENGTH];
            result_string = result[self.RLE_VALUE];

            if result_string == 'N' or result_string == 'X':
                continue

            if self._only_ignore_very_flaky and result_string not in seenResults:
                # Only consider a short-lived result if we've seen it more than once.
                # Otherwise, we include lots of false-positives due to tests that fail
                # for a couple runs and then start passing.
                # FIXME: Maybe we should make this more liberal and consider it a flake
                # even if we only see that failure once.
                seenResults[result_string] = True
                continue

            expectation = self.MAP_ENCODED_RESULT_STRING_TO_EXPECTATIONS_VALUE[result_string]
            resultsMap[expectation] = True;

        return resultsMap.keys()

    def _walk_tests_trie(self, trie, out, path_so_far=""):
        for name in trie:
            new_path = os.path.join(path_so_far, name)
            if self.RESULTS_KEY not in trie[name]:
                self._walk_tests_trie(trie[name], out, new_path)
                continue

            results = trie[name][self.RESULTS_KEY]
            actual_results = self._actual_results_for_test(results)
            if len(actual_results) > 1:
                out.append('Bug(auto) %s [ %s ]' % (new_path, " ".join(actual_results)))
