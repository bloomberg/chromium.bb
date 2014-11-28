# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for app_list.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""
INCLUDE_CPP_FILES_ONLY = (
  r'.*\.cc$', r'.*\.h$'
)

def CheckChangeLintsClean(input_api, output_api):
  """Makes sure that the ui/app_list/ code is cpplint clean."""
  sources = lambda x: input_api.FilterSourceFile(
    x, white_list = INCLUDE_CPP_FILES_ONLY, black_list = None)
  return input_api.canned_checks.CheckChangeLintsClean(
      input_api, output_api, sources)

def CheckChangeOnUpload(input_api, output_api):
  results = []
  results += CheckChangeLintsClean(input_api, output_api)
  results += input_api.canned_checks.CheckPatchFormatted(input_api, output_api)
  return results
