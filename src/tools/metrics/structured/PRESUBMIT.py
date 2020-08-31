# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for structured.xml.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

STRUCTURED_XML = 'structured.xml'

def CheckChange(input_api, output_api):
  """ Checks that structured.xml is pretty-printed and well-formatted. """
  for f in input_api.AffectedTextFiles():
    p = f.AbsoluteLocalPath()
    if (input_api.basename(p) == STRUCTURED_XML
        and input_api.os_path.dirname(p) == input_api.PresubmitLocalPath()):
      cwd = input_api.os_path.dirname(p)

      exit_code = input_api.subprocess.call(
          [input_api.python_executable, 'pretty_print.py', '--presubmit'],
          cwd=cwd)
      if exit_code != 0:
        return [
            output_api.PresubmitError(
                '%s is not prettified; run git cl format to fix.' %
                STRUCTURED_XML),
        ]

      exit_code = input_api.subprocess.call(
          [input_api.python_executable, 'validate_format.py', '--presubmit'],
          cwd=cwd)
      if exit_code != 0:
        return [
            output_api.PresubmitError(
                '%s does not pass format validation; run %s/validate_format.py '
                'and fix the reported error(s) or warning(s).' %
                (STRUCTURED_XML, input_api.PresubmitLocalPath())),
        ]

  return []

def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)
