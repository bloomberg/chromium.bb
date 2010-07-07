# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

def CheckChange(input_api, output_api):
  """Checks that the user didn't paste 'Suppression:' into the file"""
  keyword = 'Suppression:'
  for f, line_num, line in input_api.RightHandSideLines(lambda x:
      x.LocalPath().endswith('.txt')):
    if keyword in line:
      text = '"%s" must not be included; %s line %s' % (
          keyword, f.LocalPath(), line_num)
      return [output_api.PresubmitError(text)]
  return []

def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)

def GetPreferredTrySlaves():
  return ['linux_valgrind', 'mac_valgrind']
