# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for app_list.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

CC_SOURCE_FILES=(r'^cc/.*\.(cc|h)$',)

def CheckChangeLintsClean(input_api, output_api):
  input_api.cpplint._cpplint_state.ResetErrorCounts()  # reset global state
  source_filter = lambda x: input_api.FilterSourceFile(
    x, white_list=CC_SOURCE_FILES, black_list=None)
  files = [f.AbsoluteLocalPath() for f in
           input_api.AffectedSourceFiles(source_filter)]
  level = 1  # strict, but just warn

  for file_name in files:
    input_api.cpplint.ProcessFile(file_name, level)

  if not input_api.cpplint._cpplint_state.error_count:
    return []

  return [output_api.PresubmitPromptWarning(
    'Changelist failed cpplint.py check.')]

def CheckChangeOnUpload(input_api, output_api):
  results = []
  results += CheckChangeLintsClean(input_api, output_api)
  results += input_api.canned_checks.CheckPatchFormatted(input_api, output_api)
  return results
