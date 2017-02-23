# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def PostUploadHook(cl, change, output_api):
  return output_api.EnsureCQIncludeTrybotsAreAdded(
    cl,
    [
      'master.tryserver.chromium.linux:closure_compilation',
    ],
    'Automatically added optional Closure bots to run on CQ.')


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def _CommonChecks(input_api, output_api):
  return input_api.canned_checks.CheckPatchFormatted(input_api, output_api,
                                                     check_js=True)
