# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/ui/events

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

def GetPreferredTrySlaves():
  tests = set(['ash_unittests',
               'aura_unittests',
               'events_unittests',
               'keyboard_unittests',
               'views_unittests'])

  return [('linux_rel', tests),
          ('linux_chromeos', tests),
          ('linux_chromeos_asan', tests),
          ('win', tests),
          ('win_rel', tests)]
