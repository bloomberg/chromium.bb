# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for the content_shell targets."""

import cr


class ContentShellTarget(cr.NamedTarget):
  NAME = 'content_shell'
  DEFAULT = cr.Config.From(
      #  CR_URL is the page to open when the target is run.
      CR_URL='https://www.google.com/',
  )
  CONFIG = cr.Config.From(
      CR_RUN_ARGUMENTS=cr.Config.Optional('-d "{CR_URL!e}"'),
      CR_TARGET_NAME='ContentShell',
      CR_PACKAGE='org.chromium.content_shell_apk',
      CR_ACTIVITY='.ContentShellActivity',
  )


class ContentShellTestTarget(cr.NamedTarget):
  NAME = 'content_shell_test'
  CONFIG = cr.Config.From(
      CR_TARGET_NAME='ContentShellTest',
  )
