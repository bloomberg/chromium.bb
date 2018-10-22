# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def PostUploadHook(cl, change, output_api):
  """git cl upload will call this hook after the issue is created/modified.

  This hook modifies the CL description in order to run extra perf trybot
  tests (in particular, obbs fyi tests) in addition
  to the regular CQ try bots. We don't have enough hardware to run
  against all tools/perf/ commits, but should be run against changes
  likely to affect these tests.
  """
  del change  # unused
  return output_api.EnsureCQIncludeTrybotsAreAdded(
    cl,
    [
      'master.tryserver.chromium.perf:obbs_fyi',
    ],
    'Automatically added optional perf trybot tests to run on CQ.')
