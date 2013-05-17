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

from datetime import datetime
from django.utils import simplejson
import logging
import sys
import traceback

from model.testfile import TestFile

JSON_RESULTS_FILE = "results.json"
JSON_RESULTS_FILE_SMALL = "results-small.json"
JSON_RESULTS_PREFIX = "ADD_RESULTS("
JSON_RESULTS_SUFFIX = ");"
JSON_RESULTS_VERSION_KEY = "version"
JSON_RESULTS_BUILD_NUMBERS = "buildNumbers"
JSON_RESULTS_TESTS = "tests"
JSON_RESULTS_RESULTS = "results"
JSON_RESULTS_TIMES = "times"
JSON_RESULTS_PASS = "P"
JSON_RESULTS_SKIP = "X"
JSON_RESULTS_NO_DATA = "N"
JSON_RESULTS_MIN_TIME = 3
JSON_RESULTS_HIERARCHICAL_VERSION = 4
JSON_RESULTS_MAX_BUILDS = 500
JSON_RESULTS_MAX_BUILDS_SMALL = 100
FAILURES_BY_TYPE_KEY = "num_failures_by_type"
FAILURE_MAP_KEY = 'failure_map'

FAILURE_TO_CHAR = {
    "PASS": JSON_RESULTS_PASS,
    "SKIP": JSON_RESULTS_SKIP,
    "CRASH": "C",
    "TIMEOUT": "T",
    "IMAGE": "I",
    "TEXT": "F",
    "AUDIO": "A",
    "MISSING": "O",
    "IMAGE+TEXT": "Z",
    "NO DATA": JSON_RESULTS_NO_DATA,
}

# FIXME: Use dict comprehensions once we update the server to python 2.7.
CHAR_TO_FAILURE = dict((value, key) for key, value in FAILURE_TO_CHAR.items())

def _is_directory(subtree):
    return JSON_RESULTS_RESULTS not in subtree


class JsonResults(object):
    @classmethod
    def _strip_prefix_suffix(cls, data):
        if data.startswith(JSON_RESULTS_PREFIX) and data.endswith(JSON_RESULTS_SUFFIX):
            return data[len(JSON_RESULTS_PREFIX):len(data) - len(JSON_RESULTS_SUFFIX)]
        return data

    @classmethod
    def _generate_file_data(cls, json, sort_keys=False):
        return simplejson.dumps(json, separators=(',', ':'), sort_keys=sort_keys)

    @classmethod
    def _load_json(cls, file_data):
        json_results_str = cls._strip_prefix_suffix(file_data)
        if not json_results_str:
            logging.warning("No json results data.")
            return None

        try:
            return simplejson.loads(json_results_str)
        except:
            logging.debug(json_results_str)
            logging.error("Failed to load json results: %s", traceback.print_exception(*sys.exc_info()))
            return None

    @classmethod
    def _merge_json(cls, aggregated_json, incremental_json, num_runs):
        cls._merge_non_test_data(aggregated_json, incremental_json, num_runs)
        incremental_tests = incremental_json[JSON_RESULTS_TESTS]
        if incremental_tests:
            aggregated_tests = aggregated_json[JSON_RESULTS_TESTS]
            cls._merge_tests(aggregated_tests, incremental_tests, num_runs)

    @classmethod
    def _merge_non_test_data(cls, aggregated_json, incremental_json, num_runs):
        incremental_builds = incremental_json[JSON_RESULTS_BUILD_NUMBERS]
        aggregated_builds = aggregated_json[JSON_RESULTS_BUILD_NUMBERS]
        aggregated_build_number = int(aggregated_builds[0])

        # FIXME: It's no longer possible to have multiple runs worth of data in the incremental_json,
        # So we can get rid of this for-loop and the associated index.
        for index in reversed(range(len(incremental_builds))):
            build_number = int(incremental_builds[index])
            logging.debug("Merging build %s, incremental json index: %d.", build_number, index)

            # Merge this build into aggreagated results.
            cls._merge_one_build(aggregated_json, incremental_json, index, num_runs)

    @classmethod
    def _merge_one_build(cls, aggregated_json, incremental_json, incremental_index, num_runs):
        for key in incremental_json.keys():
            # Merge json results except "tests" properties (results, times etc).
            # "tests" properties will be handled separately.
            if key == JSON_RESULTS_TESTS:
                continue

            if key in aggregated_json:
                if key == FAILURES_BY_TYPE_KEY:
                    cls._merge_one_build(aggregated_json[key], incremental_json[key], incremental_index, num_runs=num_runs)
                else:
                    aggregated_json[key].insert(0, incremental_json[key][incremental_index])
                    aggregated_json[key] = aggregated_json[key][:num_runs]
            else:
                aggregated_json[key] = incremental_json[key]

    @classmethod
    def _merge_tests(cls, aggregated_json, incremental_json, num_runs):
        # FIXME: Some data got corrupted and has results/times at the directory level.
        # Once the data is fixe, this should assert that the directory level does not have
        # results or times and just return "JSON_RESULTS_RESULTS not in subtree".
        if JSON_RESULTS_RESULTS in aggregated_json:
            del aggregated_json[JSON_RESULTS_RESULTS]
        if JSON_RESULTS_TIMES in aggregated_json:
            del aggregated_json[JSON_RESULTS_TIMES]

        all_tests = set(aggregated_json.iterkeys())
        if incremental_json:
            all_tests |= set(incremental_json.iterkeys())

        for test_name in all_tests:
            if test_name not in aggregated_json:
                aggregated_json[test_name] = incremental_json[test_name]
                continue

            incremental_sub_result = incremental_json[test_name] if incremental_json and test_name in incremental_json else None
            if _is_directory(aggregated_json[test_name]):
                cls._merge_tests(aggregated_json[test_name], incremental_sub_result, num_runs)
                continue

            if incremental_sub_result:
                results = incremental_sub_result[JSON_RESULTS_RESULTS]
                times = incremental_sub_result[JSON_RESULTS_TIMES]
            else:
                results = [[1, JSON_RESULTS_NO_DATA]]
                times = [[1, 0]]

            aggregated_test = aggregated_json[test_name]
            cls._insert_item_run_length_encoded(results, aggregated_test[JSON_RESULTS_RESULTS], num_runs)
            cls._insert_item_run_length_encoded(times, aggregated_test[JSON_RESULTS_TIMES], num_runs)

    @classmethod
    def _insert_item_run_length_encoded(cls, incremental_item, aggregated_item, num_runs):
        for item in incremental_item:
            if len(aggregated_item) and item[1] == aggregated_item[0][1]:
                aggregated_item[0][0] = min(aggregated_item[0][0] + item[0], num_runs)
            else:
                aggregated_item.insert(0, item)

    @classmethod
    def _normalize_results(cls, aggregated_json, num_runs):
        names_to_delete = []
        for test_name in aggregated_json:
            if _is_directory(aggregated_json[test_name]):
                cls._normalize_results(aggregated_json[test_name], num_runs)
            else:
                leaf = aggregated_json[test_name]
                leaf[JSON_RESULTS_RESULTS] = cls._remove_items_over_max_number_of_builds(leaf[JSON_RESULTS_RESULTS], num_runs)
                leaf[JSON_RESULTS_TIMES] = cls._remove_items_over_max_number_of_builds(leaf[JSON_RESULTS_TIMES], num_runs)
                if cls._should_delete_leaf(leaf):
                    names_to_delete.append(test_name)

        for test_name in names_to_delete:
            del aggregated_json[test_name]

    @classmethod
    def _should_delete_leaf(cls, leaf):
        deletable_types = set((JSON_RESULTS_PASS, JSON_RESULTS_NO_DATA, JSON_RESULTS_SKIP))
        for result in leaf[JSON_RESULTS_RESULTS]:
            if result[1] not in deletable_types:
                return False

        for time in leaf[JSON_RESULTS_TIMES]:
            if time[1] >= JSON_RESULTS_MIN_TIME:
                return False

        return True

    @classmethod
    def _remove_items_over_max_number_of_builds(cls, encoded_list, num_runs):
        num_builds = 0
        index = 0
        for result in encoded_list:
            num_builds = num_builds + result[0]
            index = index + 1
            if num_builds >= num_runs:
                return encoded_list[:index]

        return encoded_list

    @classmethod
    def _check_json(cls, builder, json):
        version = json[JSON_RESULTS_VERSION_KEY]
        if version > JSON_RESULTS_HIERARCHICAL_VERSION:
            logging.error("Results JSON version '%s' is not supported.", version)
            return False

        if not builder in json:
            logging.error("Builder '%s' is not in json results.", builder)
            return False

        results_for_builder = json[builder]
        if not JSON_RESULTS_BUILD_NUMBERS in results_for_builder:
            logging.error("Missing build number in json results.")
            return False

        # FIXME: These keys are no longer used. Delete this code once the bots have cycled
        # and these keys are no longer in any of the aggregate JSON files.
        keys_to_delete = ['deferredCounts', 'wontfixCounts', 'webkitRevision']
        for key in keys_to_delete:
            if key in json[builder]:
                del json[builder][key]

        return True

    @classmethod
    def _populate_tests_from_full_results(cls, full_results, new_results):
        if 'expected' in full_results:
            if full_results['expected'] != 'PASS':
                new_results['expected'] = full_results['expected']
            time = int(round(full_results['time'])) if 'time' in full_results else 0
            new_results['times'] = [[1, time]]
            # FIXME: Include the retry result as well and find a nice way to display it in the flakiness dashboard.
            first_actual_failure = full_results['actual'].split(' ')[0]
            new_results['results'] = [[1, FAILURE_TO_CHAR[first_actual_failure]]]
            return

        for key in full_results:
            new_results[key] = {}
            cls._populate_tests_from_full_results(full_results[key], new_results[key])

    @classmethod
    def _convert_full_results_format_to_aggregate(cls, full_results_format):
        num_total_tests = 0
        num_failing_tests = 0
        fixableCounts = {}
        failures_by_type = full_results_format['num_failures_by_type']

        # FIXME: full_results format has "FAIL" entries, but that is no longer a possible result type.
        if 'FAIL' in failures_by_type:
            del failures_by_type['FAIL']

        for failure_type in failures_by_type:
            count = failures_by_type[failure_type]
            num_total_tests += count
            if failure_type != 'PASS':
                num_failing_tests += count
            fixableCounts[FAILURE_TO_CHAR[failure_type]] = count

        tests = {}
        cls._populate_tests_from_full_results(full_results_format['tests'], tests)

        aggregate_results_format = {
            JSON_RESULTS_VERSION_KEY: JSON_RESULTS_HIERARCHICAL_VERSION,
            full_results_format['builder_name']: {
                # FIXME: Use dict comprehensions once we update the server to python 2.7.
                'num_failures_by_type': dict((key, [value]) for key, value in failures_by_type.items()),
                'tests': tests,
                # FIXME: Have the consumers of these use num_failures_by_type directly and stop include these counts.
                'allFixableCount': [num_total_tests],
                'fixableCount': [num_failing_tests],
                'fixableCounts': [fixableCounts],
                # FIXME: Have all the consumers of this switch over to the full_results_format keys
                # so we don't have to do this silly conversion.
                JSON_RESULTS_BUILD_NUMBERS: [full_results_format['build_number']],
                'chromeRevision': [full_results_format['chromium_revision']],
                'blinkRevision': [full_results_format['blink_revision']],
                'secondsSinceEpoch': [full_results_format['seconds_since_epoch']],
            }
        }
        return aggregate_results_format

    @classmethod
    def _get_aggregated_json(cls, builder, aggregated, incremental, is_full_results_format, num_runs, sort_keys):
        if not incremental:
            logging.warning("Nothing to merge.")
            return None

        logging.info("Loading incremental json...")
        incremental_json = cls._load_json(incremental)
        if not incremental_json:
            return None

        if is_full_results_format:
            incremental_json = cls._convert_full_results_format_to_aggregate(incremental_json)

        logging.info("Checking incremental json...")
        if not cls._check_json(builder, incremental_json):
            return None

        logging.info("Loading existing aggregated json...")
        aggregated_json = cls._load_json(aggregated)
        if not aggregated_json:
            return incremental_json

        logging.info("Checking existing aggregated json...")
        if not cls._check_json(builder, aggregated_json):
            return incremental_json

        if aggregated_json[builder][JSON_RESULTS_BUILD_NUMBERS][0] == incremental_json[builder][JSON_RESULTS_BUILD_NUMBERS][0]:
            logging.error("Incremental JSON's build number is the latest build number in the aggregated JSON: %d." % aggregated_json[builder][JSON_RESULTS_BUILD_NUMBERS][0])
            return aggregated_json

        logging.info("Merging json results...")
        try:
            cls._merge_json(aggregated_json[builder], incremental_json[builder], num_runs)
        except:
            logging.error("Failed to merge json results: %s", traceback.print_exception(*sys.exc_info()))
            return None

        return aggregated_json

    @classmethod
    def merge(cls, builder, aggregated, incremental, is_full_results_format, num_runs, sort_keys=False):
        aggregated_json = cls._get_aggregated_json(builder, aggregated, incremental, is_full_results_format, num_runs, sort_keys)
        if not aggregated_json:
            return None

        aggregated_json[JSON_RESULTS_VERSION_KEY] = JSON_RESULTS_HIERARCHICAL_VERSION
        aggregated_json[FAILURE_MAP_KEY] = CHAR_TO_FAILURE
        cls._normalize_results(aggregated_json[builder][JSON_RESULTS_TESTS], num_runs)
        return cls._generate_file_data(aggregated_json, sort_keys)

    @classmethod
    def update(cls, master, builder, test_type, incremental, is_full_results_format):
        small_file_updated = cls.update_file(master, builder, test_type, incremental, is_full_results_format, JSON_RESULTS_FILE_SMALL, JSON_RESULTS_MAX_BUILDS_SMALL)
        large_file_updated = cls.update_file(master, builder, test_type, incremental, is_full_results_format, JSON_RESULTS_FILE, JSON_RESULTS_MAX_BUILDS)
        return small_file_updated and large_file_updated

    @classmethod
    def update_file(cls, master, builder, test_type, incremental, is_full_results_format, filename, num_runs):
        files = TestFile.get_files(master, builder, test_type, filename)
        if files:
            file = files[0]
        else:
            file = TestFile()
            file.master = master
            file.builder = builder
            file.test_type = test_type
            file.name = filename
            file.data = ""

        new_results = cls.merge(builder, file.data, incremental, is_full_results_format, num_runs)

        if not new_results or not file.save(new_results):
            logging.info("Update failed, master: %s, builder: %s, test_type: %s, name: %s." % (master, builder, test_type, filename))
            return False

        return True

    @classmethod
    def _delete_results_and_times(cls, tests):
        for key in tests.keys():
            if key in (JSON_RESULTS_RESULTS, JSON_RESULTS_TIMES):
                del tests[key]
            else:
                cls._delete_results_and_times(tests[key])

    @classmethod
    def get_test_list(cls, builder, json_file_data):
        logging.debug("Loading test results json...")
        json = cls._load_json(json_file_data)
        if not json:
            return None

        logging.debug("Checking test results json...")
        if not cls._check_json(builder, json):
            return None

        test_list_json = {}
        tests = json[builder][JSON_RESULTS_TESTS]
        cls._delete_results_and_times(tests)
        test_list_json[builder] = {"tests": tests}
        return cls._generate_file_data(test_list_json)
