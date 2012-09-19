# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys

PYLINT_BLACKLIST = []
PYLINT_DISABLED_WARNINGS = []

def CheckChangeOnUpload(input_api, output_api):
  report = []
  report.extend(input_api.canned_checks.PanProjectChecks(
      input_api, output_api))
  return report

def CheckChangeOnCommit(input_api, output_api):
  report = []
  report.extend(input_api.canned_checks.PanProjectChecks(input_api, output_api))

  old_sys_path = sys.path
  try:
    sys.path = [os.path.join('..', 'chrome_remote_control')] + sys.path
    report.extend(input_api.canned_checks.RunPylint(
        input_api, output_api,
        black_list=PYLINT_BLACKLIST,
        disabled_warnings=PYLINT_DISABLED_WARNINGS))
  finally:
    sys.path = old_sys_path

  return report
