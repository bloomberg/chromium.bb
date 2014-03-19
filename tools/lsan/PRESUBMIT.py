# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

import re

def CheckChange(input_api, output_api):
  errors = []

  for f in input_api.AffectedFiles():
    if not f.LocalPath().endswith('suppressions.txt'):
      continue
    for line_num, line in enumerate(f.NewContents()):
      line = line.strip()
      if line.startswith('#') or not line:
        continue
      if not line.startswith('leak:'):
        errors.append('"%s" should be "leak:..." in %s line %d' %
                      (line, f.LocalPath(), line_num))
  if errors:
    return [output_api.PresubmitError('\n'.join(errors))]
  return []

def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)

def GetPreferredTryMasters(project, change):
  return {
    'tryserver.chromium': {
      'linux_asan': set(['compile']),
      'mac_asan': set(['compile']),
    }
  }
