# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module to hold linux specific action implementations."""

import cr


class LinuxRunner(cr.Runner):
  """An implementation of cr.Runner for the linux platform.

  This supports directly executing the binaries from the output directory.
  """

  @property
  def enabled(self):
    return cr.LinuxPlatform.GetInstance().is_active

  def Kill(self, targets, arguments):
    # TODO(iancottrell): Think about how to implement this, or even if we should
    print '**WARNING** Kill not yet implemented on linux'

  def Run(self, target, arguments):
    with target:
      cr.Host.Execute('{CR_BINARY}', '{CR_RUN_ARGUMENTS}', *arguments)

  def Test(self, target, arguments):
    self.Run(target, arguments)


class LinuxInstaller(cr.Installer):
  """An implementation of cr.Installer for the linux platform.

  This does nothing, the linux runner works from the output directory, there
  is no need to install anywhere.
  """

  @property
  def enabled(self):
    return cr.LinuxPlatform.GetInstance().is_active

  def Uninstall(self, targets, arguments):
    pass

  def Install(self, targets, arguments):
    pass

  def Reinstall(self, targets, arguments):
    pass

