# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""LayoutTests/ presubmit script for Blink.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

import filecmp


def _CheckTestharnessResults(input_api, output_api):
    """Checks for testharness.js test baseline files that contain only PASS lines.

    In general these files are unnecessary because for testharness.js tests, if there is
    no baseline file then the test is considered to pass when the output is all PASS.
    """
    baseline_files = _TestharnessBaselineFilesToCheck(input_api)
    if not baseline_files:
        return []

    checker_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
        '..', 'Tools', 'Scripts', 'check-testharness-expected-pass')

    args = [input_api.python_executable, checker_path]
    args.extend(baseline_files)
    _, errs = input_api.subprocess.Popen(args,
        stdout=input_api.subprocess.PIPE,
        stderr=input_api.subprocess.PIPE).communicate()
    if errs:
        return [output_api.PresubmitError(errs)]
    return []


def _TestharnessBaselineFilesToCheck(input_api):
    """Returns a list of paths of -expected.txt files for testharness.js tests."""
    baseline_files = []
    for f in input_api.AffectedFiles():
        if f.Action() == 'D':
            continue
        path = f.AbsoluteLocalPath()
        if not path.endswith('-expected.txt'):
            continue
        if (input_api.os_path.join('LayoutTests', 'platform') in path or
            input_api.os_path.join('LayoutTests', 'virtual') in path):
            # We want to ignore files in LayoutTests/platform, because some all-PASS
            # platform specific baselines may be necessary to prevent fallback to a
            # more general baseline; we also ignore files in LayoutTests/virtual
            # for a similar reason; some all-pass baselines are necessary to
            # prevent fallback to the corresponding non-virtual test baseline.
            continue
        baseline_files.append(path)
    return baseline_files


def _CheckIdenticalFiles(input_api, output_api):
    """Verifies that certain files are identical in various locations.

    These files should always be updated together. If this list is modified,
    consider also changing the list of files to copy from web-platform-tests
    when importing in Tools/Scripts/webkitpy/deps_updater.py.
    """
    dirty_files = set(input_api.LocalPaths())

    groups = [
        ('external/wpt/resources/idlharness.js', 'resources/idlharness.js'),
        ('external/wpt/resources/testharness.js', 'resources/testharness.js'),
    ]

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
