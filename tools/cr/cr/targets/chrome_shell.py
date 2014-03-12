# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for the chrome_shell targets."""

import cr


class ChromeShellTarget(cr.NamedTarget):
  NAME = 'chrome_shell'
  DEFAULT = cr.Config.From(
      #  CR_URL is the page to open when the target is run.
      CR_URL='https://www.google.com/',
  )
  CONFIG = cr.Config.From(
      CR_RUN_ARGUMENTS=cr.Config.Optional('-d "{CR_URL!e}"'),
      CR_TARGET_NAME='ChromeShell',
      CR_PACKAGE='org.chromium.chrome.shell',
      CR_ACTIVITY='.ChromeShellActivity',
  )


class ChromeShellTestTarget(cr.NamedTarget):
  NAME = 'chrome_shell_test'
  CONFIG = cr.Config.From(
      CR_TARGET_NAME='ChromiumTestShellTest',
  )

