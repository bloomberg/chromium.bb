# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates TestExpectations based on results in builder bots.

Scans the TestExpectations file and uses results from actual builder bots runs
to remove tests that are marked as flaky but don't fail in the specified way.

E.g. If a test has this expectation:
    bug(test) fast/test.html [ Failure Pass ]

And all the runs on builders have passed the line will be removed.

Additionally, the runs don't all have to be Passing to remove the line;
as long as the non-Passing results are of a type not specified in the
expectation this line will be removed. For example, if this is the
expectation:

    bug(test) fast/test.html [ Crash Pass ]

But the results on the builders show only Passes and Timeouts, the line
will be removed since there's no Crash results.
"""

import argparse
import logging

from webkitpy.layout_tests.models.test_expectations import TestExpectations

_log = logging.getLogger(__name__)


def main(host, bot_test_expectations_factory, argv):
    parser = argparse.ArgumentParser(epilog=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    parser.parse_args(argv)

    port = host.port_factory.get()

    logging.basicConfig(level=logging.INFO, format="%(message)s")

    expectations_file = port.path_to_generic_test_expectations_file()
    if not host.filesystem.isfile(expectations_file):
        _log.warn("Didn't find generic expectations file at: " + expectations_file)
        return 1

    remove_flakes_o_matic = RemoveFlakesOMatic(host,
                                               port,
                                               bot_test_expectations_factory)

    test_expectations = remove_flakes_o_matic.get_updated_test_expectations()

    remove_flakes_o_matic.write_test_expectations(test_expectations,
                                                  expectations_file)
    return 0


class RemoveFlakesOMatic(object):
    def __init__(self, host, port, bot_test_expectations_factory):
        self._host = host
        self._port = port
        self._expectations_factory = bot_test_expectations_factory
        self.builder_results_by_path = {}

    def get_updated_test_expectations(self):
        """Filters out passing lines from TestExpectations file.

        Reads the current TestExpectations file and, using results from the
        build bots, removes lines that are passing. That is, removes lines that
        were not needed to keep the bots green.

        Returns:
            A TestExpectations object with the passing lines filtered out.
        """
        test_expectations = TestExpectations(self._port, include_overrides=False).expectations()
        # TODO: Filter the expectations based on results.
        return test_expectations

    def write_test_expectations(self, test_expectations, test_expectations_file):
        """Writes the given TestExpectations object to the filesystem.

        Args:
            test_expectations: The TestExpectations object to write.
            test_expectations_file: The full file path of the Blink
                TestExpectations file. This file will be overwritten.
        """
        self._host.filesystem.write_text_file(
            test_expectations_file,
            TestExpectations.list_to_string(test_expectations, reconstitute_only_these=[]))
