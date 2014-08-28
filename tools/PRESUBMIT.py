# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for auto-bisect.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""

import imp
import os

# Paths to bisect config files relative to src/tools.
CONFIG_FILES = [
    'auto_bisect/config.cfg',
    'run-perf-test.cfg'
]

PYLINT_BLACKLIST = []
PYLINT_DISABLED_WARNINGS = []


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def _CommonChecks(input_api, output_api):
  """Does all presubmit checks for auto-bisect."""
  # TODO(qyearsley) Run bisect unit test.
  # TODO(qyearsley) Run pylint on all auto-bisect py files but not other files.
  results = []
  results.extend(_CheckAllConfigFiles(input_api, output_api))
  return results


def _CheckAllConfigFiles(input_api, output_api):
  """Checks all bisect config files and returns a list of presubmit results."""
  results = []
  for f in input_api.AffectedFiles():
    for config_file in CONFIG_FILES:
      if f.LocalPath().endswith(config_file):
        results.extend(_CheckConfigFile(config_file, output_api))
  return results


def _CheckConfigFile(file_path, output_api):
  """Checks one bisect config file and returns a list of presubmit results."""
  try:
    config_file = imp.load_source('config', file_path)
  except IOError as e:
    warning = 'Failed to read config file %s: %s' % (file_path, str(e))
    return [output_api.PresubmitError(warning, items=[file_path])]

  if not hasattr(config_file.config):
    warning = 'Config file has no "config" global variable: %s' % str(e)
    return [output_api.PresubmitError(warning, items=[file_path])]

  if type(config_file.config) is not dict:
    warning = 'Config file "config" global variable is not dict: %s' % str(e)
    return [output_api.PresubmitError(warning, items=[file_path])]

  for k, v in config_dict.iteritems():
    if v != '':
      warning = 'Non-empty value in config dict: %s: %s' % (repr(k), repr(v))
      warning += ('\nThe bisection config file should only contain a config '
                  'dict with empty fields. Changes to this file should not '
                  'be submitted.')
      return [output_api.PresubmitError(warning, items=[file_path])]

  return []
