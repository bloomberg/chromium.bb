# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""LayoutTests/ presubmit script for Blink.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""


def _CheckTestharnessResults(input_api, output_api):
    expected_files = [f.AbsoluteLocalPath() for f in input_api.AffectedFiles() if f.LocalPath().endswith('-expected.txt') and f.Action() != 'D']
    if len(expected_files) == 0:
        return []

    checker_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
        '..', 'Tools', 'Scripts', 'check-testharness-expected-pass')

    args = [input_api.python_executable, checker_path]
    args.extend(expected_files)
    _, errs = input_api.subprocess.Popen(args,
        stdout=input_api.subprocess.PIPE,
        stderr=input_api.subprocess.PIPE).communicate()
    if errs:
        return [output_api.PresubmitError(errs)]
    return []


def CheckChangeOnUpload(input_api, output_api):
    results = []
    results.extend(_CheckTestharnessResults(input_api, output_api))
    return results


def CheckChangeOnCommit(input_api, output_api):
    results = []
    results.extend(_CheckTestharnessResults(input_api, output_api))
    return results
