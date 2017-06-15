# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Lint functionality duplicated from web-platform-tests upstream.

This is to catch lint errors that would otherwise be caught in WPT CI.
See http://web-platform-tests.org/writing-tests/lint-tool.html for more
information about the lint tool.
"""


def _LintWPT(input_api, output_api):
    wpt_path = input_api.os_path.join(input_api.PresubmitLocalPath(), 'wpt')
    linter_path = input_api.os_path.join(
        input_api.PresubmitLocalPath(), '..', '..', 'Tools', 'Scripts',
        'webkitpy', 'thirdparty', 'wpt', 'wpt', 'lint')

    paths_in_wpt = []
    for f in input_api.AffectedFiles():
        abs_path = f.AbsoluteLocalPath()
        if abs_path.startswith(wpt_path):
            paths_in_wpt.append(abs_path[len(wpt_path) + 1:])

    # If there are changes in LayoutTests/external that aren't in wpt, e.g.
    # changes to wpt_automation or this presubmit script, then we can return
    # to avoid running the linter on all files in wpt (which is slow).
    if not paths_in_wpt:
        return []

    args = [
        input_api.python_executable,
        linter_path,
        '--repo-root=%s' % wpt_path,
        '--ignore-glob=*-expected.txt',
    ] + paths_in_wpt

    proc = input_api.subprocess.Popen(
        args,
        stdout=input_api.subprocess.PIPE,
        stderr=input_api.subprocess.PIPE)
    stdout, _ = proc.communicate()

    if stdout:
        return [output_api.PresubmitError(stdout)]
    return []


def CheckChangeOnUpload(input_api, output_api):
    return _LintWPT(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _LintWPT(input_api, output_api)
