# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""LayoutTests/ presubmit script for Blink.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

import filecmp


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


def _CheckIdenticalFiles(input_api, output_api):
    """Verifies that certain files are identical in various locations.
    These files should always be updated together."""

    dirty_files = set(input_api.LocalPaths())

    groups = [[
        'imported/web-platform-tests/resources/testharness.js',
        'resources/testharness.js',
    ], [
        'imported/web-platform-tests/resources/testharnessreport.js',
        'resources/testharnessreport.js',
    ], [
        'imported/web-platform-tests/resources/idlharness.js',
        'resources/idlharness.js',
    ]]

    def _absolute_path(s):
        return input_api.os_path.join(input_api.PresubmitLocalPath(), *s.split('/'))

    def _local_path(s):
        return input_api.os_path.join('third_party', 'WebKit', 'LayoutTests', *s.split('/'))

    errors = []
    for group in groups:
        if any(_local_path(p) in dirty_files for p in group):
            a = group[0]
            for b in group[1:]:
                if not filecmp.cmp(_absolute_path(a), _absolute_path(b), shallow=False):
                    errors.append(output_api.PresubmitError(
                        'Files that should match differ: (see https://crbug.com/362788)\n' +
                        '  %s <=> %s' % (_local_path(a), _local_path(b))))
    return errors


def CheckChangeOnUpload(input_api, output_api):
    results = []
    results.extend(_CheckTestharnessResults(input_api, output_api))
    results.extend(_CheckIdenticalFiles(input_api, output_api))
    return results


def CheckChangeOnCommit(input_api, output_api):
    results = []
    results.extend(_CheckTestharnessResults(input_api, output_api))
    results.extend(_CheckIdenticalFiles(input_api, output_api))
    return results
