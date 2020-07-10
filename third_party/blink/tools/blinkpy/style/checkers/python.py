# Copyright (C) 2010 Chris Jerdonek (cjerdonek@webkit.org)
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Supports checking WebKit style in Python files."""

import os
import re
import sys


from blinkpy.common.path_finder import PathFinder
from blinkpy.common.path_finder import get_blink_tools_dir
from blinkpy.common.path_finder import get_blinkpy_thirdparty_dir
from blinkpy.common.system.filesystem import FileSystem
from blinkpy.common.system.executive import Executive
from blinkpy.third_party import pep8


class PythonChecker(object):
    """Processes text lines for checking style."""

    def __init__(self, file_path, handle_style_error):
        self._file_path = file_path
        self._handle_style_error = handle_style_error

    def check(self, lines_unused=None):
        self._check_pep8()
        self._check_pylint()

    def _check_pep8(self):
        # Initialize pep8.options, which is necessary for
        # Checker.check_all() to execute.
        pep8.process_options(arglist=[self._file_path])

        pep8_checker = pep8.Checker(self._file_path)

        def _pep8_handle_error(line_number, offset, text, check):
            # FIXME: Incorporate the character offset into the error output.
            #        This will require updating the error handler __call__
            #        signature to include an optional "offset" parameter.
            pep8_code = text[:4]
            pep8_message = text[5:]

            category = 'pep8/' + pep8_code

            self._handle_style_error(line_number, category, 5, pep8_message)

        pep8_checker.report_error = _pep8_handle_error
        pep8_checker.check_all()

    def _check_pylint(self):
        output = self.run_pylint(self._file_path)
        errors = self._parse_pylint_output(output)
        for line_number, category, message in errors:
            self._handle_style_error(line_number, category, 5, message)

    def run_pylint(self, path):
        finder = PathFinder(FileSystem())
        executive = Executive()
        env = os.environ.copy()
        env['PYTHONPATH'] = os.pathsep.join([
            get_blink_tools_dir(),
            finder.path_from_blink_source('build', 'scripts'),
            get_blinkpy_thirdparty_dir(),
            finder.path_from_blink_source('bindings', 'scripts'),
            finder.path_from_chromium_base('build', 'android'),
            finder.path_from_chromium_base('third_party'),  # for jinja2
            finder.path_from_chromium_base('third_party', 'catapult', 'devil'),
            finder.path_from_chromium_base('third_party', 'pymock'),
            finder.path_from_chromium_base('tools'),
        ])
        return executive.run_command([
            sys.executable,
            finder.path_from_depot_tools_base('pylint'),
            '--output-format=parseable',
            '--rcfile=' + finder.path_from_blink_tools('blinkpy', 'pylintrc'),
            path,
        ], env=env, error_handler=executive.ignore_error)

    def _parse_pylint_output(self, output):
        # We filter out these messages because they are bugs in pylint that produce false positives.
        # FIXME: Does it make sense to combine these rules with the rules in style/checker.py somehow?
        FALSE_POSITIVES = [
            # possibly http://www.logilab.org/ticket/98613 ?
            "Instance of 'Popen' has no 'poll' member",
            "Instance of 'Popen' has no 'returncode' member",
            "Instance of 'Popen' has no 'stdin' member",
            "Instance of 'Popen' has no 'stdout' member",
            "Instance of 'Popen' has no 'stderr' member",
            "Instance of 'Popen' has no 'wait' member",
        ]

        lint_regex = re.compile(r'([^:]+):([^:]+): \[([^]]+)\] (.*)')
        errors = []
        for line in output.splitlines():
            if any(msg in line for msg in FALSE_POSITIVES):
                continue

            match_obj = lint_regex.match(line)
            if not match_obj:
                continue

            line_number = int(match_obj.group(2))
            category_and_method = match_obj.group(3).split(', ')
            category = 'pylint/' + (category_and_method[0])
            if len(category_and_method) > 1:
                message = '[%s] %s' % (category_and_method[1], match_obj.group(4))
            else:
                message = match_obj.group(4)
            errors.append((line_number, category, message))
        return errors
