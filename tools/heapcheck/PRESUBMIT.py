# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

def CheckChange(input_api, output_api):
  """Checks the memcheck suppressions files for bad data."""
  suppressions = {}
  errors = []
  skip_next_line = False
  func_re = input_api.re.compile('[a-z_.]+\(.+\)$')
  for f, line_num, line in input_api.RightHandSideLines(lambda x:
      x.LocalPath().endswith('.txt')):
    line = line.lstrip()
    if line.startswith('#') or not line:
      continue

    if skip_next_line:
      if suppressions.has_key(line):
        errors.append('suppression with name "%s" at %s line %s has already '
                      'been defined at line %s' % (line, f.LocalPath(),
                                                   line_num,
                                                   suppressions[line][1]))
      else:
        suppressions[line] = (f, line_num)
      skip_next_line = False
      continue
    if line == '{':
      skip_next_line = True
      continue
    if (line.startswith('fun:') or line.startswith('obj:') or
        line == 'Heapcheck:Leak' or line == '}' or
        line == '...'):
      continue
    if func_re.match(line):
      continue
    errors.append('"%s" is probably wrong: %s line %s' % (line, f.LocalPath(),
                                                          line_num))
  if errors:
    return [output_api.PresubmitError('\n'.join(errors))]
  return []

def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)
