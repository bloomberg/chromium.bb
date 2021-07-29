# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for the content/browser/conversions directory.

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

USE_PYTHON3 = True

def CheckConversionStorageSchemaModification(input_api, output_api):
  """ Checks the kCurrentVersionNumber is modified when necessary.

  Whenever any of the following files is changed:
    - conversion_storage_sql.cc
    - conversion_storage_sql_migrations.cc
    - rate_limit_table.cc
  and kCurrentVersionNumber stays intact, this check returns a
  presubmit warning to make sure the value is updated if necessary.
  """

  database_files_changed = False
  database_version_changed = False

  for affected_file in input_api.AffectedFiles():
    basename = input_api.basename(affected_file.LocalPath())

    if (basename == 'conversion_storage_sql_migrations.cc' or
        basename == 'conversion_storage_sql.cc' or
        basename == 'rate_limit_table.cc'):
      database_files_changed = True

    if basename == 'conversion_storage_sql.cc':
      for (_, line) in affected_file.ChangedContents():
        if 'const int kCurrentVersionNumber' in line:
          database_version_changed = True
          break

  out = []
  if database_files_changed and not database_version_changed:
    out.append(output_api.PresubmitPromptWarning(
        'Please make sure that the conversions database is properly versioned '
        'and migrated when making changes to schema or table contents. '
        'kCurrentVersionNumber in conversion_storage_sql.cc '
        'must be updated when doing a migration.'))
  return out

def CheckChangeOnUpload(input_api, output_api):
  return CheckConversionStorageSchemaModification(input_api, output_api)
