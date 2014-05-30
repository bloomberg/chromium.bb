# Copyright (c) 2011 Google Inc. All rights reserved.
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

import optparse
from webkitpy.tool.multicommandtool import AbstractDeclarativeCommand
from webkitpy.layout_tests.layout_package.bot_test_expectations import BotTestExpectationsFactory
from webkitpy.layout_tests.models.test_expectations import TestExpectationParser, TestExpectationsModel, TestExpectations


class FlakyTests(AbstractDeclarativeCommand):
    name = "update-flaky-tests"
    help_text = "Update FlakyTests file from the flakiness dashboard"
    show_in_main_help = True

    def __init__(self):
        options = [
            optparse.make_option('--upload', action='store_true',
                help='upload the changed FlakyTest file for review'),
        ]
        AbstractDeclarativeCommand.__init__(self, options=options)

    def execute(self, options, args, tool):
        port = tool.port_factory.get()
        model = TestExpectationsModel()
        for port_name in tool.port_factory.all_port_names():
            expectations = BotTestExpectationsFactory().expectations_for_port(port_name)
            for line in expectations.expectation_lines(only_ignore_very_flaky=True):
                model.add_expectation_line(line)
        # FIXME: We need an official API to get all the test names or all test lines.
        lines = model._test_to_expectation_line.values()
        lines.sort(key=lambda line: line.path)
        # Skip any tests which are mentioned in the dashboard but not in our checkout:
        fs = tool.filesystem
        lines = filter(lambda line: fs.exists(fs.join(port.layout_tests_dir(), line.path)), lines)
        flaky_tests_path = fs.join(port.layout_tests_dir(), 'FlakyTests')
        # Note: This includes all flaky tests from the dashboard, even ones mentioned
        # in existing TestExpectations. We could certainly load existing TestExpecations
        # and filter accordingly, or update existing TestExpectations instead of FlakyTests.
        with open(flaky_tests_path, 'w') as flake_file:
            flake_file.write(TestExpectations.list_to_string(lines))

        if not options.upload:
            return 0

        files = tool.scm().changed_files()
        flaky_tests_path = 'LayoutTests/FlakyTests'
        if flaky_tests_path not in files:
            print "%s is not changed, not uploading." % flaky_tests_path
            return 0

        commit_message = "Update FlakyTests"
        git_cmd = ['git', 'commit', '-m', commit_message, flaky_tests_path]
        tool.executive.run_command(git_cmd)

        git_cmd = ['git', 'cl', 'upload', '--use-commit-queue', '--send-mail']
        tool.executive.run_command(git_cmd)
        # If there are changes to git, upload.
