#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script used to generate the tests definitions for Web App testing framework.
See the README.md file in this directory for more information.

Usage: python3 chrome/test/web_apps/generate_framework_tests_and_coverage.py
"""

import argparse
from io import TextIOWrapper
import logging
import os
from typing import List, Optional, Dict
import csv

from models import ActionCoverage
from models import ActionNode
from models import CoverageTestsByPlatform
from models import CoverageTestsByPlatformSet
from models import TestPartitionDescription
from models import TestPlatform
from models import CoverageTest
from graph_analysis import build_action_node_graph
from graph_analysis import generate_coverage_file_and_percents
from graph_analysis import trim_graph_to_platform_actions
from graph_analysis import generate_framework_tests
from graph_analysis import generage_graphviz_dot_file
from file_reading import find_existing_and_disabled_tests, read_platform_supported_actions
from file_reading import read_actions_file
from file_reading import read_unprocessed_coverage_tests_file
from test_analysis import compare_and_print_tests_to_remove_and_add
from test_analysis import expand_parameterized_tests
from test_analysis import filter_coverage_tests_for_platform
from test_analysis import partition_framework_tests_per_platform_combination


def check_partition_prefixes(partition_a: TestPartitionDescription,
                             partition_b: TestPartitionDescription):
    if partition_a.test_file_prefix.startswith(partition_b.test_file_prefix):
        raise ValueError(
            f"Cannot have partition filenames intersect:"
            f"{partition_a.test_file_prefix} and {partition_b.test_file_prefix}"
        )


def generate_framework_tests_and_coverage(
        supported_framework_action_file: TextIOWrapper,
        actions_file: TextIOWrapper, coverage_required_file: TextIOWrapper,
        custom_partitions: List[TestPartitionDescription],
        default_partition: TestPartitionDescription, coverage_output_dir: str,
        graph_output_dir: Optional[str]):

    for partition_a in custom_partitions:
        check_partition_prefixes(partition_a, default_partition)
        for partition_b in custom_partitions:
            if partition_a == partition_b:
                continue
            check_partition_prefixes(partition_a, partition_b)
    actions_csv = csv.reader(actions_file, delimiter=',')
    platform_supported_actions = read_platform_supported_actions(
        csv.reader(supported_framework_action_file))
    (actions, action_base_name_to_default_param) = read_actions_file(
        actions_csv, platform_supported_actions)

    coverage_csv = csv.reader(coverage_required_file, delimiter=',')
    required_coverage_tests = read_unprocessed_coverage_tests_file(
        coverage_csv, actions, action_base_name_to_default_param)

    required_coverage_tests = expand_parameterized_tests(
        required_coverage_tests)

    if graph_output_dir:
        coverage_root_node = ActionNode.CreateRootNode()
        build_action_node_graph(coverage_root_node, required_coverage_tests)
        graph_file = generage_graphviz_dot_file(coverage_root_node, None)
        output_coverage_graph_file_name = os.path.join(
            graph_output_dir, "coverage_required_graph.dot")
        with open(output_coverage_graph_file_name, "w",
                  encoding="'utf-8") as coverage_graph_file:
            coverage_graph_file.write("# This is a generated file.\n")
            coverage_graph_file.write(graph_file)
            coverage_graph_file.close()

    # Each platform can have unique tests. Start by generating the required
    # tests per platform, and the generated testes per platform.
    required_coverage_by_platform: CoverageTestsByPlatform = {}
    generated_tests_by_platform: CoverageTestsByPlatform = {}
    for platform in TestPlatform:
        platform_tests = filter_coverage_tests_for_platform(
            required_coverage_tests.copy(), platform)
        required_coverage_by_platform[platform] = platform_tests

        generated_tests_root_node = ActionNode.CreateRootNode()
        build_action_node_graph(generated_tests_root_node, platform_tests)
        trim_graph_to_platform_actions(generated_tests_root_node, platform)
        generated_tests_by_platform[platform] = generate_framework_tests(
            generated_tests_root_node, platform)
        if graph_output_dir:
            graph_file = generage_graphviz_dot_file(generated_tests_root_node,
                                                    platform)
            output_coverage_graph_file_name = os.path.join(
                graph_output_dir,
                "generated_tests_graph_" + platform.suffix + ".dot")
            with open(output_coverage_graph_file_name, "w",
                      encoding="'utf-8") as coverage_graph_file:
                coverage_graph_file.write("# This is a generated file.\n")
                coverage_graph_file.write(graph_file)

    # A test can be required to run on on multiple platforms, and we group
    # required tests by platform set to output minimal number of browser tests
    # files. This allows the test to exist only in one place for ease of
    # sheriffing. Example:
    # Linux:    testA, testB
    # Mac:      testA, testB
    # Windows:  testA
    # ChromeOS: testA, testC
    # ->
    # {Linux, Mac, Windows, ChromeOS} -> testA
    # {Linux, Mac} -> testB
    # {ChromeOS} -> testC
    required_coverage_by_platform_set: CoverageTestsByPlatformSet = (
        partition_framework_tests_per_platform_combination(
            generated_tests_by_platform))

    # Find all existing tests.
    all_partitions = [default_partition]
    all_partitions.extend(custom_partitions)
    (existing_tests_ids_by_platform_set, disabled_test_ids_by_platform
     ) = find_existing_and_disabled_tests(all_partitions)

    # Print all diffs that are required.
    compare_and_print_tests_to_remove_and_add(
        existing_tests_ids_by_platform_set, required_coverage_by_platform_set,
        custom_partitions, default_partition)

    # To calculate coverage we need to incorporate any disabled tests.
    # Remove any disabled tests from the generated tests per platform.
    for platform, tests in generated_tests_by_platform.items():
        disabled_tests = disabled_test_ids_by_platform.get(platform, [])
        tests_minus_disabled: List[CoverageTest] = []
        for test in tests:
            if test.id not in disabled_tests:
                tests_minus_disabled.append(test)
            else:
                logging.info("Removing disabled test from coverage: " +
                             test.id)
        generated_tests_root_node = ActionNode.CreateRootNode()
        build_action_node_graph(generated_tests_root_node,
                                tests_minus_disabled)
        (coverage_file, full, partial) = generate_coverage_file_and_percents(
            required_coverage_by_platform[platform], generated_tests_root_node,
            platform)
        coverage_filename = os.path.join(coverage_output_dir,
                                         f"coverage_{platform.suffix}.tsv")
        with open(coverage_filename, 'w+', encoding="'utf-8") as file:
            file.write("# This is a generated file.\n")
            file.write(f"# Full coverage: {full:.0%}, "
                       f"with partial coverage: {partial:.0%}\n")
            file.write(coverage_file + "\n")
    return


def main():
    parser = argparse.ArgumentParser(description='WebApp Test List Processor')
    parser.add_argument('-v',
                        dest='v',
                        action='store_true',
                        help='Include info logging.',
                        required=False)

    parser.add_argument('--graphs',
                        dest='graphs',
                        action='store_true',
                        help='Output dot graphs from all steps.',
                        required=False)
    options = parser.parse_args()
    logging.basicConfig(level=logging.INFO if options.v else logging.WARN,
                        format='[%(asctime)s %(levelname)s] %(message)s',
                        datefmt='%H:%M:%S')
    script_dir = os.path.dirname(os.path.realpath(__file__))
    actions_filename = os.path.join(script_dir, "data", "actions.csv")
    supported_actions_filename = os.path.join(
        script_dir, "data", "framework_supported_actions.csv")
    coverage_required_filename = os.path.join(script_dir, "data",
                                              "coverage_required.csv")
    coverage_output_dir = os.path.join(script_dir, "coverage")

    default_tests_location = os.path.join(script_dir, "..", "..", "browser",
                                          "ui", "views", "web_apps")
    sync_tests_location = os.path.join(script_dir, "..", "..", "browser",
                                       "sync", "test", "integration")

    # These describe where existing browsertests are to be found, and where the
    # script runner will be directed to write tests to.
    custom_partitions = [
        TestPartitionDescription(
            action_name_prefixes={"switch_profile_clients", "sync_"},
            browsertest_dir=sync_tests_location,
            test_file_prefix="two_client_web_apps_integration_test",
            test_fixture="TwoClientWebAppsIntegrationTest")
    ]
    default_partition = TestPartitionDescription(
        action_name_prefixes=set(),
        browsertest_dir=default_tests_location,
        test_file_prefix="web_app_integration_browsertest",
        test_fixture="WebAppIntegrationBrowserTest")

    graph_output_dir = None
    if options.graphs:
        graph_output_dir = script_dir

    with open(actions_filename, 'r', encoding="utf-8") as actions_file, \
            open(supported_actions_filename, 'r', encoding="utf-8") \
                as supported_actions, \
            open(coverage_required_filename, 'r', encoding="utf-8") \
                as coverage_file:
        generate_framework_tests_and_coverage(supported_actions, actions_file,
                                              coverage_file, custom_partitions,
                                              default_partition,
                                              coverage_output_dir,
                                              graph_output_dir)


if __name__ == '__main__':
    main()
