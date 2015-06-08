# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for remoting.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

BROWSER_TEST_INSTRUCTIONS_LINK = (
    "https://wiki.corp.google.com/twiki/bin/view/Main/ChromotingWaterfall#Running_on_a_Swarming_bot.")


def CheckChangeOnUpload(input_api, output_api):
  print "*******IMPORTANT NOTE*******"
  print "Before committing, please run Remoting browser_tests."
  print "Instructions: %s" % BROWSER_TEST_INSTRUCTIONS_LINK
  print "Make sure all tests pass."
  return []


def CheckChangeOnCommit(input_api, output_api):
  """TODO(anandc): Run browser-tests on the Chromoting waterfall as part of
  committing a CL. See http://crbug/498026"""
  return []
