# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile

from measurements import session_restore
from measurements import session_restore_with_url
from profile_creators import small_profile_creator
from telemetry import test
from telemetry.page import profile_generator


class _SessionRestoreTest(test.Test):

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    super(_SessionRestoreTest, cls).ProcessCommandLineArgs(parser, args)
    profile_type = 'small_profile'
    if not args.browser_options.profile_dir:
      profile_dir = os.path.join(tempfile.gettempdir(), profile_type)
      if not os.path.exists(profile_dir):
        new_args = args.Copy()
        new_args.pageset_repeat = 1
        new_args.output_dir = profile_dir
        profile_generator.GenerateProfiles(
            small_profile_creator.SmallProfileCreator, profile_type, new_args)
      args.browser_options.profile_dir = os.path.join(profile_dir, profile_type)


@test.Disabled('android')  # crbug.com/325479
class SessionRestoreColdTypical25(_SessionRestoreTest):
  tag = 'cold'
  test = session_restore.SessionRestore
  page_set = 'page_sets/typical_25.py'
  options = {'cold': True,
             'pageset_repeat': 5}


@test.Disabled('android')  # crbug.com/325479
class SessionRestoreWarmTypical25(_SessionRestoreTest):
  tag = 'warm'
  test = session_restore.SessionRestore
  page_set = 'page_sets/typical_25.py'
  options = {'warm': True,
             'pageset_repeat': 20}


@test.Disabled('android')  # crbug.com/325479
class SessionRestoreWithUrlCold(_SessionRestoreTest):
  """Measure Chrome cold session restore with startup URLs."""
  tag = 'cold'
  test = session_restore_with_url.SessionRestoreWithUrl
  page_set = 'page_sets/startup_pages.py'
  options = {'cold': True,
             'pageset_repeat': 5}


@test.Disabled('android')  # crbug.com/325479
class SessionRestoreWithUrlWarm(_SessionRestoreTest):
  """Measure Chrome warm session restore with startup URLs."""
  tag = 'warm'
  test = session_restore_with_url.SessionRestoreWithUrl
  page_set = 'page_sets/startup_pages.py'
  options = {'warm': True,
             'pageset_repeat': 10}
