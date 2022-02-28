# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import sys

assert sys.version_info[0] == 3

from blinkpy.web_tests.stale_expectation_removal import builders
from blinkpy.web_tests.stale_expectation_removal import data_types
from blinkpy.web_tests.stale_expectation_removal import expectations
from blinkpy.web_tests.stale_expectation_removal import queries
from unexpected_passes_common import argument_parsing
from unexpected_passes_common import builders as common_builders
from unexpected_passes_common import data_types as common_data_types
from unexpected_passes_common import result_output


def ParseArgs():
    parser = argparse.ArgumentParser(description=(
        'Script for finding cases of stale expectations that can be '
        'removed/modified.'))
    argument_parsing.AddCommonArguments(parser)
    args = parser.parse_args()
    argument_parsing.SetLoggingVerbosity(args)
    return args


def main():
    args = ParseArgs()
    # Set any custom data types.
    common_data_types.SetExpectationImplementation(
        data_types.WebTestExpectation)
    common_data_types.SetResultImplementation(data_types.WebTestResult)
    common_data_types.SetBuildStatsImplementation(data_types.WebTestBuildStats)
    common_data_types.SetTestExpectationMapImplementation(
        data_types.WebTestTestExpectationMap)

    builders_instance = builders.WebTestBuilders()
    common_builders.RegisterInstance(builders_instance)
    expectations_instance = expectations.WebTestExpectations()

    test_expectation_map = expectations_instance.CreateTestExpectationMap(
        expectations_instance.GetExpectationFilepaths(), None,
        args.expectation_grace_period)
    ci_builders = builders_instance.GetCiBuilders(None)

    querier = queries.WebTestBigQueryQuerier(None, args.project,
                                             args.num_samples,
                                             args.large_query_mode)
    # Unmatched results are mainly useful for script maintainers, as they don't
    # provide any additional information for the purposes of finding
    # unexpectedly passing tests or unused expectations.
    unmatched = querier.FillExpectationMapForCiBuilders(
        test_expectation_map, ci_builders)
    try_builders = builders_instance.GetTryBuilders(ci_builders)
    unmatched.update(
        querier.FillExpectationMapForTryBuilders(test_expectation_map,
                                                 try_builders))
    unused_expectations = test_expectation_map.FilterOutUnusedExpectations()
    stale, semi_stale, active = test_expectation_map.SplitByStaleness()
    result_output.OutputResults(stale, semi_stale, active, unmatched,
                                unused_expectations, args.output_format)

    affected_urls = set()
    stale_message = ''
    if args.remove_stale_expectations:
        for expectation_file, expectation_map in stale.items():
            affected_urls |= expectations_instance.RemoveExpectationsFromFile(
                expectation_map.keys(), expectation_file)
            stale_message += (
                'Stale expectations removed from %s. Stale '
                'comments, etc. may still need to be removed.\n' %
                expectation_file)
        for expectation_file, unused_list in unused_expectations.items():
            affected_urls |= expectations_instance.RemoveExpectationsFromFile(
                unused_list, expectation_file)
            stale_message += (
                'Unused expectations removed from %s. Stale comments, etc. '
                'may still need to be removed.\n' % expectation_file)

    if args.modify_semi_stale_expectations:
        affected_urls |= expectations_instance.ModifySemiStaleExpectations(
            semi_stale)
        stale_message += ('Semi-stale expectations modified in expectation '
                          'files. Stale comments, etc. may still need to be '
                          'removed.\n')

    if stale_message:
        print(stale_message)
    if affected_urls:
        orphaned_urls = expectations_instance.FindOrphanedBugs(affected_urls)
        result_output.OutputAffectedUrls(affected_urls, orphaned_urls)

    return 0


if __name__ == '__main__':
    sys.exit(main())
