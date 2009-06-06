#!/usr/bin/env python
# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generic presubmit checks that can be reused by other presubmit checks."""


def CheckChangeHasTestField(input_api, output_api):
  """Requires that the changelist have a TEST= field."""
  if input_api.change.TEST:
    return []
  else:
    return [output_api.PresubmitNotifyResult(
        "Changelist should have a TEST= field. TEST=none is allowed.")]


def CheckChangeHasBugField(input_api, output_api):
  """Requires that the changelist have a BUG= field."""
  if input_api.change.BUG:
    return []
  else:
    return [output_api.PresubmitNotifyResult(
        "Changelist should have a BUG= field. BUG=none is allowed.")]


def CheckChangeHasTestedField(input_api, output_api):
  """Requires that the changelist have a TESTED= field."""
  if input_api.change.TESTED:
    return []
  else:
    return [output_api.PresubmitError("Changelist must have a TESTED= field.")]


def CheckChangeHasQaField(input_api, output_api):
  """Requires that the changelist have a QA= field."""
  if input_api.change.QA:
    return []
  else:
    return [output_api.PresubmitError("Changelist must have a QA= field.")]


def CheckDoNotSubmitInDescription(input_api, output_api):
  """Checks that the user didn't add 'DO NOT ' + 'SUBMIT' to the CL description.
  """
  keyword = 'DO NOT ' + 'SUBMIT'
  if keyword in input_api.change.DescriptionText():
    return [output_api.PresubmitError(
        keyword + " is present in the changelist description.")]
  else:
    return []


def CheckDoNotSubmitInFiles(input_api, output_api):
  """Checks that the user didn't add 'DO NOT ' + 'SUBMIT' to any files."""
  keyword = 'DO NOT ' + 'SUBMIT'
  for f, line_num, line in input_api.RightHandSideLines():
    if keyword in line:
      text = 'Found ' + keyword + ' in %s, line %s' % (f.LocalPath(), line_num)
      return [output_api.PresubmitError(text)]
  return []


def CheckDoNotSubmit(input_api, output_api):
  return (
      CheckDoNotSubmitInDescription(input_api, output_api) +
      CheckDoNotSubmitInFiles(input_api, output_api)
      )


def CheckChangeHasNoTabs(input_api, output_api):
  """Checks that there are no tab characters in any of the text files to be
  submitted.
  """
  for f, line_num, line in input_api.RightHandSideLines():
    if '\t' in line:
      return [output_api.PresubmitError(
          "Found a tab character in %s, line %s" %
          (f.LocalPath(), line_num))]
  return []


def CheckLongLines(input_api, output_api, maxlen=80):
  """Checks that there aren't any lines longer than maxlen characters in any of
  the text files to be submitted.
  """
  bad = []
  for f, line_num, line in input_api.RightHandSideLines():
    if len(line) > maxlen:
      bad.append(
          '%s, line %s, %s chars' %
          (f.LocalPath(), line_num, len(line)))
      if len(bad) == 5:  # Just show the first 5 errors.
        break

  if bad:
    msg = "Found lines longer than %s characters (first 5 shown)." % maxlen
    return [output_api.PresubmitPromptWarning(msg, items=bad)]
  else:
    return []


def CheckTreeIsOpen(input_api, output_api, url, closed):
  """Checks that an url's content doesn't match a regexp that would mean that
  the tree is closed."""
  try:
    connection = input_api.urllib2.urlopen(url)
    status = connection.read()
    connection.close()
    if input_api.re.match(closed, status):
      long_text = status + '\n' + url
      return [output_api.PresubmitError("The tree is closed.",
                                        long_text=long_text)]
  except IOError:
    pass
  return []


def _RunPythonUnitTests_LoadTests(input_api, module_name):
  """Meant to be stubbed out during unit testing."""
  module = __import__(module_name)
  for part in module_name.split('.')[1:]:
    module = getattr(module, part)
  return input_api.unittest.TestLoader().loadTestsFromModule(module)._tests

def RunPythonUnitTests(input_api, output_api, unit_tests):
  """Imports the unit_tests modules and run them."""
  tests_suite = []
  outputs = []
  for unit_test in unit_tests:
    try:
      tests_suite.extend(_RunPythonUnitTests_LoadTests(unit_test))
    except ImportError:
      outputs.append(output_api.PresubmitError("Failed to load %s" % unit_test))

  results = input_api.unittest.TextTestRunner(verbosity=0).run(
      input_api.unittest.TestSuite(tests_suite))
  if not results.wasSuccessful():
    outputs.append(output_api.PresubmitError(
        "%d unit tests failed." % (results.failures + results.errors)))
  return outputs
