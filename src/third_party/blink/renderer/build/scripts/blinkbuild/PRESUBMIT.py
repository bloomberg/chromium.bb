# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function


def _RunBindingsTests(input_api, output_api):
    pardir = input_api.os_path.pardir
    cmd_name = 'run_bindings_tests.py'
    run_bindings_tests_path = input_api.os_path.join(
        input_api.PresubmitLocalPath(), *([pardir] * 4 + ['tools', cmd_name]))
    cmd = [input_api.python_executable, run_bindings_tests_path]
    if input_api.verbose:
        print('Running ' + cmd_name)
    else:
        cmd.append('--suppress-diff')
    test_cmd = input_api.Command(
        name=cmd_name, cmd=cmd, kwargs={}, message=output_api.PresubmitError)
    return input_api.RunTests([test_cmd])


def CheckChangeOnUpload(input_api, output_api):
    return _RunBindingsTests(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _RunBindingsTests(input_api, output_api)
