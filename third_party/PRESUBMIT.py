# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def _CheckThirdPartyReadmesUpdated(input_api, output_api):
  """
  Checks to make sure that README.chromium files are properly updated
  when dependancies in third_party are modified.
  """
  readmes = []
  files = []
  errors = []
  for f in input_api.AffectedFiles():
    if f.LocalPath().startswith('third_party' + input_api.os_path.sep):
      files.append(f)
      if f.LocalPath().endswith("README.chromium"):
        readmes.append(f)
  if files and not readmes:
    errors.append(output_api.PresubmitPromptWarning(
       'When updating or adding third party code the appropriate\n'
       '\'README.chromium\' file should also be updated with the correct\n'
       'version and package information.', files))
  if not readmes:
    return errors

  name_pattern = input_api.re.compile(
    r'^Name: [a-zA-Z0-9_\-\. ]+\r?$',
    input_api.re.IGNORECASE | input_api.re.MULTILINE)
  shortname_pattern = input_api.re.compile(
    r'^Short Name: [a-zA-Z0-9_\-\.]+\r?$',
    input_api.re.IGNORECASE | input_api.re.MULTILINE)
  version_pattern = input_api.re.compile(
    r'^Version: [a-zA-Z0-9_\-\.:]+\r?$',
    input_api.re.IGNORECASE | input_api.re.MULTILINE)
  release_pattern = input_api.re.compile(
    r'Security Critical: (yes)|(no)\r?$',
    input_api.re.IGNORECASE | input_api.re.MULTILINE)

  for f in readmes:
    contents = input_api.ReadFile(f)
    if (not shortname_pattern.search(contents)
        and not name_pattern.search(contents)):
      errors.append(output_api.PresubmitError(
        'Third party README files should contain either a \'Short Name\' or\n'
        'a \'Name\' which is the name under which the package is\n'
        'distributed. Check README.chromium.template for details.',
        [f]))
    if not version_pattern.search(contents):
      errors.append(output_api.PresubmitError(
        'Third party README files should contain a \'Version\' field.\n'
        'If the package is not versioned or the version is not known\n'
        'list the version as \'unknown\'.\n'
        'Check README.chromium.template for details.',
        [f]))
    if not release_pattern.search(contents):
      errors.append(output_api.PresubmitError(
        'Third party README files should contain a \'Security Critical\'\n'
        'field. This field specifies whether the package is built with\n'
        'Chromium. Check README.chromium.template for details.',
        [f]))
  return errors


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CheckThirdPartyReadmesUpdated(input_api, output_api))
  return results
