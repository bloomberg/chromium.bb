# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A script to modify TestExpectations lines based layout test failures in try jobs.

This script outputs a list of test expectation lines to add to a 'TestExpectations' file
by retrieving the try job results for the current CL.
"""

import logging

from webkitpy.common.net import buildbot
from webkitpy.common.net import rietveld


_log = logging.getLogger(__name__)


def main(host, port):
    expectations_file = port.path_to_generic_test_expectations_file()
    expectations_line_adder = W3CExpectationsLineAdder(host)
    issue_number = expectations_line_adder.get_issue_number()
    try_bots = expectations_line_adder.get_try_bots()
    try_jobs = rietveld.latest_try_jobs(issue_number, try_bots, host.web)
    line_expectations_dict = {}
    if not try_jobs:
        print 'No Try Job information was collected.'
        return 1
    for try_job in try_jobs:
        builder_name = try_job[0]
        build_number = try_job[1]
        builder = buildbot.Builder(builder_name, host.buildbot)
        build = buildbot.Build(builder, build_number)
        platform_results_dict = expectations_line_adder.get_failing_results_dict(builder, build)
        line_expectations_dict = expectations_line_adder.merge_dicts(line_expectations_dict, platform_results_dict)
    for platform_results_dicts in line_expectations_dict.values():
        platform_results_dicts = expectations_line_adder.merge_same_valued_keys(platform_results_dicts)
    line_list = expectations_line_adder.create_line_list(line_expectations_dict)
    expectations_line_adder.write_to_test_expectations(host, expectations_file, line_list)


class W3CExpectationsLineAdder(object):

    def __init__(self, host):
        self._host = host
        self.filesystem = host.filesystem

    def get_issue_number(self):
        return self._host.scm().get_issue_number()

    def get_try_bots(self):
        return self._host.builders.all_try_builder_names()

    def _generate_results_dict(self, platform, result_list):
        test_dict = {}
        if '-' in platform:
            platform = platform[platform.find('-') + 1:].capitalize()
        for result in result_list:
            test_dict[result.test_name()] = {
                platform: {
                    'expected': result.expected_results(),
                    'actual': result.actual_results(),
                    'bug': 'crbug.com/626703'
                }}
        return test_dict

    def get_failing_results_dict(self, builder, build):
        """Returns a nested dict of failing test results.

        Retrieves a full list of layout test results from a builder result URL. Collects
        the builder name, platform and a list of tests that did not run as expected.

        Args:
            builder: A Builder object.
            build: A Build object.

        Returns:
            A dictionary with the structure: {
                'key': {
                    'expected': 'TIMEOUT',
                    'actual': 'CRASH',
                    'bug': 'crbug.com/11111'
                }
            }
        """
        layout_test_results = builder.fetch_layout_test_results(build.results_url())
        builder_name = layout_test_results.builder_name()
        platform = self._host.builders.port_name_for_builder_name(builder_name)
        result_list = layout_test_results.didnt_run_as_expected_results()
        failing_results_dict = self._generate_results_dict(platform, result_list)
        return failing_results_dict

    def merge_dicts(self, target, source, path=None):
        """Recursively merge nested dictionaries, returning the target dictionary

        Merges the keys and values from the source dict into the target dict.

        Args:
            target: A dictionary where the keys and values from source are updated
                in.
            source: A dictionary that provides the new keys and values to be added
                into target.

        Returns:
            An updated target dictionary.
        """
        path = path or []
        for key in source:
            if key in target:
                if (isinstance(target[key], dict)) and isinstance(source[key], dict):
                    self.merge_dicts(target[key], source[key], path + [str(key)])
                elif target[key] == source[key]:
                    pass
                else:
                    raise ValueError('The key: %s already exist in the target dictionary.' % '.'.join(path))
            else:
                target[key] = source[key]
        return target

    def merge_same_valued_keys(self, dictionary):
        """Merges keys in dictionary with same value.

        Traverses through a dict and compares the values of keys to one another.
        If the values match, the keys are combined to a tuple and the previous keys
        are removed from the dict.

        Args:
            dictionary: A dictionary with a dictionary as the value.

        Returns:
            A dictionary with updated keys to reflect matching values of keys.
            Example: {
                'one': {'foo': 'bar'},
                'two': {'foo': 'bar'},
                'three': {'foo': bar'}
            }
            is converted to {('one', 'two', 'three'): {'foo': 'bar'}}
        """
        matching_value_keys = set()
        keys = dictionary.keys()
        is_last_item = False
        for index, item in enumerate(keys):
            if is_last_item:
                break
            for i in range(index + 1, len(keys)):
                next_item = keys[i]
                if dictionary[item] == dictionary[next_item]:
                    matching_value_keys.update([item, next_item])
                    dictionary[tuple(matching_value_keys)] = dictionary[item]
                    is_last_item = next_item == keys[-1]
                    del dictionary[item]
                    del dictionary[next_item]
        return dictionary

    def get_expectations(self, results):
        """Returns a list of test expectations for a given test dict.

        Returns a list of one or more test expectations based on the expected
        and actual results of a given test name.

        Args:
            results: A dictionary that maps one test to its results. Example: {
                'test_name': {
                    'expected': 'PASS',
                    'actual': 'FAIL',
                    'bug': 'crbug.com/11111'
                }
            }

        Returns:
            A list of one or more test expectations with the first letter capitalized. Example:
            ['Failure', 'Timeout']
        """
        expectations = []
        failure_expectations = ['TEXT', 'FAIL', 'IMAGE+TEXT', 'IMAGE']
        pass_crash_timeout = ['TIMEOUT', 'CRASH', 'PASS']
        if results['expected'] in pass_crash_timeout and results['actual'] in failure_expectations:
            expectations.append('Failure')
        if results['expected'] in failure_expectations and results['actual'] in pass_crash_timeout:
            expectations.append(results['actual'].capitalize())
        if results['expected'] in pass_crash_timeout and results['actual'] in pass_crash_timeout:
            expectations.append(results['actual'].capitalize())
            expectations.append(results['expected'].capitalize())
        return expectations

    def create_line_list(self, merged_results):
        """Creates list of test expectations lines.

        Traverses through a merged_results and parses the value to create a test
        expectations line per key.

        Args:
            merged_results: A merged_results with the format {
                'test_name': {
                    'platform': {
                        'expected: 'PASS',
                        'actual': 'FAIL',
                        'bug': 'crbug.com/11111'
                    }
                }
            }
                It is possible for the dicitonary to have many test_name
                keys.

        Returns:
            A list of test expectations lines with the format
            ['BUG_URL [PLATFORM(S)] TEST_MAME [EXPECTATION(S)]']
        """
        line_list = []
        for test_name, platform_results in merged_results.iteritems():
            for platform in platform_results:
                platform_list = []
                bug = []
                expectations = []
                if isinstance(platform, tuple):
                    platform_list = list(platform)
                else:
                    platform_list.append(platform)
                bug.append(platform_results[platform]['bug'])
                expectations = self.get_expectations(platform_results[platform])
                line = '%s [ %s ] %s [ %s ]' % (bug[0], ' '.join(platform_list), test_name, ' '.join(expectations))
                line_list.append(str(line))
        return line_list

    def write_to_test_expectations(self, host, path, line_list):
        """Writes to TestExpectations.

        Writes to the test expectations lines in line_list
        to LayoutTest/TestExpectations. Checks the file for the string
        '# Tests added from W3C auto import bot' and writes expectation
        lines directly under it. If not found, it writes to the end of
        the file.

        Args:
            host: A Host object.
            path: The path to the file LayoutTest/TestExpectations.
            line_list: A list of w3c test expectations lines.

        Returns:
            Writes to a file on the filesystem called LayoutTests/TestExpectations.
        """
        comment_line = '# Tests added from W3C auto import bot'
        file_contents = host.filesystem.read_text_file(path)
        w3c_comment_line_index = file_contents.find(comment_line)
        all_lines = ''
        for line in line_list:
            all_lines += str(line) + '\n'
        all_lines = all_lines[:-1]
        if w3c_comment_line_index == -1:
            file_contents += '\n%s\n' % comment_line
            file_contents += all_lines
        else:
            end_of_comment_line = (file_contents[w3c_comment_line_index:].find('\n')) + w3c_comment_line_index
            new_data = file_contents[: end_of_comment_line + 1] + all_lines + file_contents[end_of_comment_line:]
            file_contents = new_data
        host.filesystem.write_text_file(path, file_contents)
