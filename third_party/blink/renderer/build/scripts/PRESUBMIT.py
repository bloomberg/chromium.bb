# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def _RunJson5Tests(input_api, output_api):
    # Skip if there is no change in json5_generator.py or tests/ folder
    white_list = [r'.*json5_generator.*', r'.*\btests[\\\/].*']
    if not input_api.AffectedFiles(file_filter=lambda x: input_api.FilterSourceFile(x, white_list=white_list)):
        return []

    if input_api.is_committing:
        message_type = output_api.PresubmitError
    else:
        message_type = output_api.PresubmitPromptWarning

    json5_generator_unittest = 'json5_generator_unittest.py'
    json5_generator_tests_path = input_api.os_path.join(
        input_api.PresubmitLocalPath(), json5_generator_unittest)
    if input_api.is_windows:
        cmd = [input_api.python_executable, json5_generator_tests_path]
    else:
        cmd = [json5_generator_tests_path]

    test_cmd = input_api.Command(
        name=json5_generator_unittest,
        cmd=cmd,
        kwargs={},
        message=message_type)
    if input_api.verbose:
        print('Running ' + json5_generator_unittest)
    return input_api.RunTests([test_cmd])


def CheckChangeOnUpload(input_api, output_api):
    return _RunJson5Tests(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _RunJson5Tests(input_api, output_api)