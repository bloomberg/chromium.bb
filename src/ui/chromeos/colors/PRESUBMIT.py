# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

STYLE_VAR_GEN_INPUTS = [r'^ui[\\/]chromeos[\\/]colors[\\/].+\.json5$']


def _CommonChecks(input_api, output_api):
    results = []
    try:
        import sys
        old_sys_path = sys.path[:]
        sys.path += [
            input_api.os_path.join(input_api.change.RepositoryRoot(), 'tools')
        ]
        import style_variable_generator.presubmit_support
        results += (
            style_variable_generator.presubmit_support.FindDeletedCSSVariables(
                input_api, output_api, STYLE_VAR_GEN_INPUTS))
    finally:
        sys.path = old_sys_path
    return results


def CheckChangeOnUpload(input_api, output_api):
    return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CommonChecks(input_api, output_api)
