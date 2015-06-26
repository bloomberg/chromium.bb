# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit tests for /tools/vim.

Runs Python unit tests in /tools/vim/tests on upload.
"""

def CheckChangeOnUpload(input_api, output_api):
  return input_api.canned_checks.RunUnitTestsInDirectory(
      input_api, output_api, 'tests', whitelist=r'.*test.py')
