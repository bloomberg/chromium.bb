# Copyright (C) 2012 Google Inc. All rights reserved.
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

import json
import logging
import optparse
import traceback

from webkitpy.common.host import Host
from webkitpy.common import exit_codes
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.port.factory import platform_options
from webkitpy.w3c.wpt_manifest import WPTManifest

_log = logging.getLogger(__name__)


def lint(host, options):
    ports_to_lint = [host.port_factory.get(name) for name in host.port_factory.all_port_names(options.platform)]
    files_linted = set()

    # In general, the set of TestExpectation files should be the same for
    # all ports. However, the method used to list expectations files is
    # in Port, and the TestExpectations constructor takes a Port.
    # Perhaps this function could be changed to just use one Port
    # (the default Port for this host) and it would work the same.

    failures = []
    for port_to_lint in ports_to_lint:
        expectations_dict = port_to_lint.all_expectations_dict()

        # There are some TestExpectations files that are not loaded by default
        # in any Port, and are instead passed via --additional-expectations on
        # some builders. We also want to inspect these files if they're present.
        extra_files = (
            'ASANExpectations',
            'LeakExpectations',
            'MSANExpectations',
        )
        for name in extra_files:
            path = port_to_lint.layout_tests_dir() + '/' + name
            if host.filesystem.exists(path):
                expectations_dict[path] = host.filesystem.read_text_file(path)

        for expectations_file in expectations_dict:

            if expectations_file in files_linted:
                continue

            try:
                test_expectations.TestExpectations(port_to_lint,
                                                   expectations_dict={expectations_file: expectations_dict[expectations_file]},
                                                   is_lint_mode=True)
            except test_expectations.ParseError as error:
                _log.error('')
                for warning in error.warnings:
                    _log.error(warning)
                    failures.append('%s: %s' % (expectations_file, warning))
                _log.error('')
            files_linted.add(expectations_file)
    return failures


def check_virtual_test_suites(host, options):
    port = host.port_factory.get(options=options)
    fs = host.filesystem
    layout_tests_dir = port.layout_tests_dir()
    virtual_suites = port.virtual_test_suites()

    failures = []
    for suite in virtual_suites:
        comps = [layout_tests_dir] + suite.name.split('/') + ['README.txt']
        path_to_readme = fs.join(*comps)
        if not fs.exists(path_to_readme):
            failure = 'LayoutTests/%s/README.txt is missing (each virtual suite must have one).' % suite.name
            _log.error(failure)
            failures.append(failure)
    if failures:
        _log.error('')
    return failures


def set_up_logging(logging_stream):
    logger = logging.getLogger()
    logger.setLevel(logging.INFO)
    handler = logging.StreamHandler(logging_stream)
    logger.addHandler(handler)
    return (logger, handler)


def tear_down_logging(logger, handler):
    logger.removeHandler(handler)


def run_checks(host, options, logging_stream):
    logger, handler = set_up_logging(logging_stream)
    try:
        failures = []
        failures.extend(lint(host, options))
        failures.extend(check_virtual_test_suites(host, options))

        if options.json:
            with open(options.json, 'w') as f:
                json.dump(failures, f)

        if failures:
            _log.error('Lint failed.')
            return 1
        else:
            _log.info('Lint succeeded.')
            return 0
    finally:
        logger.removeHandler(handler)


def main(argv, _, stderr):
    parser = optparse.OptionParser(option_list=platform_options(use_globs=True))
    parser.add_option('--json', help='Path to JSON output file')
    options, _ = parser.parse_args(argv)

    if options.platform and 'test' in options.platform:
        # It's a bit lame to import mocks into real code, but this allows the user
        # to run tests against the test platform interactively, which is useful for
        # debugging test failures.
        from webkitpy.common.host_mock import MockHost
        host = MockHost()
    else:
        host = Host()

    # Need to generate MANIFEST.json since some expectations correspond to WPT
    # tests that aren't files and only exist in the manifest.
    _log.info('Generating MANIFEST.json for web-platform-tests ...')
    WPTManifest.ensure_manifest(host)

    try:
        exit_status = run_checks(host, options, stderr)
    except KeyboardInterrupt:
        exit_status = exit_codes.INTERRUPTED_EXIT_STATUS
    except Exception as error:  # pylint: disable=broad-except
        print >> stderr, '\n%s raised: %s' % (error.__class__.__name__, error)
        traceback.print_exc(file=stderr)
        exit_status = exit_codes.EXCEPTIONAL_EXIT_STATUS

    return exit_status
