# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import unittest
import StringIO

from collections import OrderedDict

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.layout_tests.builder_list import BuilderList
from webkitpy.layout_tests.port.factory import PortFactory
from webkitpy.layout_tests.port.test import LAYOUT_TEST_DIR
from webkitpy.layout_tests.update_test_expectations import main
from webkitpy.layout_tests.update_test_expectations import RemoveFlakesOMatic

logger = logging.getLogger()
logger.level = logging.DEBUG


class FakeBotTestExpectations(object):

    def __init__(self, results_by_path):
        self._results = results_by_path

    def all_results_by_path(self):
        return self._results


class FakeBotTestExpectationsFactory(object):

    def __init__(self):
        """
        The distinct results seen in at least one run of the test.
        E.g. if the bot results for mytest.html are:
            PASS PASS FAIL PASS TIMEOUT
        then _all_results_by_builder would be:
            {
                'WebKit Linux' : {
                    'mytest.html': ['FAIL', 'PASS', 'TIMEOUT']
                }
            }
        """
        self._all_results_by_builder = {}

    def expectations_for_builder(self, builder):
        if builder not in self._all_results_by_builder:
            return None

        return FakeBotTestExpectations(self._all_results_by_builder[builder])


class FakePortFactory(PortFactory):

    def __init__(self, host):
        super(FakePortFactory, self).__init__(host)
        self._all_build_types = ()
        self._all_systems = ()
        self._configuration_specifier_macros = {
            'mac': ['mac10.10'],
            'win': ['win7'],
            'linux': ['precise']
        }

    def get(self, port_name=None, options=None, **kwargs):
        """Returns an object implementing the Port interface.

        This fake object will always return the 'test' port factory.
        """
        port = super(FakePortFactory, self).get('test', None)
        port.all_build_types = self._all_build_types
        port.all_systems = self._all_systems
        port.configuration_specifier_macros_dict = (
            self._configuration_specifier_macros)
        return port


class UpdateTestExpectationsTest(unittest.TestCase):

    def setUp(self):
        self._host = MockHost()
        self._port = self._host.port_factory.get('test', None)
        self._expectation_factory = FakeBotTestExpectationsFactory()
        self._flake_remover = RemoveFlakesOMatic(self._host,
                                                 self._port,
                                                 self._expectation_factory)
        self._port.configuration_specifier_macros_dict = {
            'mac': ['mac10.10'],
            'win': ['win7'],
            'linux': ['precise']
        }
        filesystem = self._host.filesystem
        self._write_tests_into_filesystem(filesystem)

        self._log_output = StringIO.StringIO()
        self._stream_handler = logging.StreamHandler(self._log_output)
        logger.addHandler(self._stream_handler)

    def tearDown(self):
        logger.removeHandler(self._stream_handler)
        self._log_output.close()

    def _write_tests_into_filesystem(self, filesystem):
        test_list = ['test/a.html',
                     'test/b.html',
                     'test/c.html',
                     'test/d.html',
                     'test/e.html',
                     'test/f.html',
                     'test/g.html']
        for test in test_list:
            path = filesystem.join(LAYOUT_TEST_DIR, test)
            filesystem.write_binary_file(path, '')

    def _assert_expectations_match(self, expectations, expected_string):
        self.assertIsNotNone(expectations)
        stringified_expectations = "\n".join(
            x.to_string() for x in expectations)
        expected_string = "\n".join(
            x.strip() for x in expected_string.split("\n"))
        self.assertEqual(stringified_expectations, expected_string)

    def _parse_expectations(self, expectations):
        """Parses a TestExpectation file given as string.

        This function takes a string representing the contents of the
        TestExpectations file and parses it, producing the TestExpectations
        object and sets it on the Port object where the script will read it
        from.

        Args:
            expectations: A string containing the contents of the
            TestExpectations file to use.
        """
        expectations_dict = OrderedDict()
        expectations_dict['expectations'] = expectations
        self._port.expectations_dict = lambda: expectations_dict

    def _define_builders(self, builders_dict):
        """Defines the available builders for the test.

        Args:
            builders_dict: A dictionary containing builder names to their
            attributes, see BuilderList.__init__ for the format.
        """
        self._host.builders = BuilderList(builders_dict)

    def test_dont_remove_non_flakes(self):
        """Tests that lines that aren't flaky are not touched.

        Lines are flaky if they contain a PASS as well as at least one other
        failing result.
        """
        test_expectations_before = """
            # Even though the results show all passing, none of the
            # expectations are flaky so we shouldn't remove any.
            Bug(test) test/a.html [ Pass ]
            Bug(test) test/b.html [ Timeout ]
            Bug(test) test/c.html [ Failure Timeout ]
            Bug(test) test/d.html [ Rebaseline ]
            Bug(test) test/e.html [ NeedsManualRebaseline ]
            Bug(test) test/f.html [ NeedsRebaseline ]"""

        self._define_builders({
            "WebKit Linux": {
                "port_name": "linux-precise",
                "specifiers": ['Precise', 'Release']
            },
        })
        self._port.all_build_types = ('release',)
        self._port.all_systems = (('precise', 'x86_64'),)

        self._parse_expectations(test_expectations_before)
        self._expectation_factory._all_results_by_builder = {
            'WebKit Linux': {
                "test/a.html": ["PASS", "PASS"],
                "test/b.html": ["PASS", "PASS"],
                "test/c.html": ["PASS", "PASS"],
                "test/d.html": ["PASS", "PASS"],
                "test/e.html": ["PASS", "PASS"],
                "test/f.html": ["PASS", "PASS"],
            }
        }
        updated_expectations = (
            self._flake_remover.get_updated_test_expectations())
        self._assert_expectations_match(
            updated_expectations, test_expectations_before)

    def test_dont_remove_skip(self):
        """Tests that lines with Skip are untouched.

        If a line is marked as Skip, it will eventually contain no results,
        which is indistinguishable from "All Passing" so don't remove since we
        don't know what the results actually are.
        """
        test_expectations_before = """
            # Skip expectations should never be removed.
            Bug(test) test/a.html [ Skip ]
            Bug(test) test/b.html [ Skip ]
            Bug(test) test/c.html [ Skip ]"""

        self._define_builders({
            "WebKit Linux": {
                "port_name": "linux-precise",
                "specifiers": ['Precise', 'Release']
            },
        })
        self._port.all_build_types = ('release',)
        self._port.all_systems = (('precise', 'x86_64'),)

        self._parse_expectations(test_expectations_before)
        self._expectation_factory._all_results_by_builder = {
            'WebKit Linux': {
                "test/a.html": ["PASS", "PASS"],
                "test/b.html": ["PASS", "IMAGE"],
            }
        }
        updated_expectations = (
            self._flake_remover.get_updated_test_expectations())
        self._assert_expectations_match(
            updated_expectations, test_expectations_before)

    def test_dont_remove_rebaselines(self):
        """Tests that lines with rebaseline expectations are untouched."""
        test_expectations_before = """
            # Even though the results show all passing, none of the
            # expectations are flaky so we shouldn't remove any.
            Bug(test) test/a.html [ Failure NeedsRebaseline Pass ]
            Bug(test) test/b.html [ Failure Pass Rebaseline ]
            Bug(test) test/c.html [ Failure NeedsManualRebaseline Pass ]"""

        self._define_builders({
            "WebKit Linux": {
                "port_name": "linux-precise",
                "specifiers": ['Precise', 'Release']
            },
        })
        self._port.all_build_types = ('release',)
        self._port.all_systems = (('precise', 'x86_64'),)

        self._parse_expectations(test_expectations_before)
        self._expectation_factory._all_results_by_builder = {
            'WebKit Linux': {
                "test/a.html": ["PASS", "PASS"],
                "test/b.html": ["PASS", "PASS"],
                "test/c.html": ["PASS", "PASS"]
            }
        }
        updated_expectations = (
            self._flake_remover.get_updated_test_expectations())
        self._assert_expectations_match(
            updated_expectations, test_expectations_before)

    def test_all_failure_case(self):
        """Tests that results with all failures are not treated as non-flaky."""
        test_expectations_before = (
            """# Keep since it's all failures.
            Bug(test) test/a.html [ Failure Pass ]""")

        self._define_builders({
            "WebKit Linux": {
                "port_name": "linux-precise",
                "specifiers": ['Precise', 'Release']
            },
        })
        self._port.all_build_types = ('release',)
        self._port.all_systems = (('precise', 'x86_64'),)

        self._parse_expectations(test_expectations_before)
        self._expectation_factory._all_results_by_builder = {
            'WebKit Linux': {
                "test/a.html": ["IMAGE", "IMAGE", "IMAGE"],
            }
        }
        updated_expectations = (
            self._flake_remover.get_updated_test_expectations())
        self._assert_expectations_match(updated_expectations, (
            """# Keep since it's all failures.
            Bug(test) test/a.html [ Failure Pass ]"""))

    def test_empty_test_expectations(self):
        """Running on an empty TestExpectations file outputs an empty file."""
        test_expectations_before = ""

        self._define_builders({
            "WebKit Linux": {
                "port_name": "linux-precise",
                "specifiers": ['Precise', 'Release']
            },
        })
        self._port.all_build_types = ('release',)
        self._port.all_systems = (('precise', 'x86_64'),)

        self._parse_expectations(test_expectations_before)
        self._expectation_factory._all_results_by_builder = {
            'WebKit Linux': {
                "test/a.html": ["PASS", "PASS", "PASS"],
            }
        }
        updated_expectations = (
            self._flake_remover.get_updated_test_expectations())
        self._assert_expectations_match(updated_expectations, "")

    def test_harness_no_expectations(self):
        """Tests behavior when TestExpectations file doesn't exist.

        Tests that a warning is outputted if the TestExpectations file
        doesn't exist."""

        # Setup the mock host and port.
        host = MockHost()
        host.port_factory = FakePortFactory(host)

        # Write the test file but not the TestExpectations file.
        test_expectation_path = (
            host.port_factory.get().path_to_generic_test_expectations_file())
        host.filesystem = MockFileSystem()
        self._write_tests_into_filesystem(host.filesystem)

        # Write out the fake builder bot results.
        expectation_factory = FakeBotTestExpectationsFactory()
        expectation_factory._all_results_by_builder = {}

        self.assertFalse(host.filesystem.isfile(test_expectation_path))

        return_code = main(host, expectation_factory, [])

        self.assertEqual(return_code, 1)

        expected_warning = (
            "Didn't find generic expectations file at: " +
            test_expectation_path + "\n")
        self.assertEqual(self._log_output.getvalue(), expected_warning)
        self.assertFalse(host.filesystem.isfile(test_expectation_path))
