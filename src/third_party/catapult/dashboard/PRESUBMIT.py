# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function
from __future__ import division
from __future__ import absolute_import


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def _CommonChecks(input_api, output_api):
  files_to_skip = input_api.DEFAULT_FILES_TO_SKIP + (
      '.*_pb2.py$',
      '.*bq_export.*',
  )
  results = []
  results += input_api.RunTests(
      input_api.canned_checks.GetPylint(
          input_api,
          output_api,
          extra_paths_list=_GetPathsToPrepend(input_api),
          files_to_skip=files_to_skip,
          pylintrc='pylintrc'))
  return results


def _GetPathsToPrepend(input_api):
  project_dir = input_api.PresubmitLocalPath()
  catapult_dir = input_api.os_path.join(project_dir, '..')
  return [
      project_dir,
      input_api.os_path.join(catapult_dir, 'third_party', 'mock'),
  ]
