# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for ui.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

INCLUDE_CPP_FILES_ONLY = (
  r'.*\.(cc|h|mm)$',
)

def CheckScopedPtr(input_api, output_api,
                   white_list=INCLUDE_CPP_FILES_ONLY, black_list=None):
  black_list = tuple(black_list or input_api.DEFAULT_BLACK_LIST)
  source_file_filter = lambda x: input_api.FilterSourceFile(x,
                                                            white_list,
                                                            black_list)
  errors = []
  for f in input_api.AffectedSourceFiles(source_file_filter):
    for line_number, line in f.ChangedContents():
      # Disallow:
      # return scoped_ptr<T>(foo);
      # bar = scoped_ptr<T>(foo);
      # But allow:
      # return scoped_ptr<T[]>(foo);
      # bar = scoped_ptr<T[]>(foo);
      if input_api.re.search(
              r'(=|\breturn)\s*scoped_ptr<[^\[\]>]+>\([^)]+\)', line):
        errors.append(output_api.PresubmitError(
            ('%s:%d uses explicit scoped_ptr constructor. ' +
             'Use make_scoped_ptr() instead.') % (f.LocalPath(), line_number)))
      # TODO(sky): this incorrectly catches templates. Fix and reenable.
      # Disallow:
      # scoped_ptr<T>()
      # if input_api.re.search(r'\bscoped_ptr<.*?>\(\)', line):
      #   errors.append(output_api.PresubmitError(
      #       '%s:%d uses scoped_ptr<T>(). Use nullptr instead.' %
      #       (f.LocalPath(), line_number)))
  return errors


def CheckChange(input_api, output_api):
  results = []
  results += CheckScopedPtr(input_api, output_api)
  return results


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)
