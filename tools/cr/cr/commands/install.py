# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the install command."""

import cr


class InstallCommand(cr.Command):
  """The implementation of the install command.

  This first uses Builder.Build to bring the target up to date, and then
  installs it using Installer.Reinstall.
  The builder installs its command line arguments, and you can use those to
  select which builder is used. Selecting the skip builder
  (using --builder=skip) bypasses the build stage.
  """

  def __init__(self):
    super(InstallCommand, self).__init__()
    self.help = 'Install a binary'

  def AddArguments(self, subparsers):
    parser = super(InstallCommand, self).AddArguments(subparsers)
    cr.Builder.AddArguments(self, parser)
    cr.Installer.AddArguments(self, parser)
    cr.Target.AddArguments(self, parser, allow_multiple=True)
    self.ConsumeArgs(parser, 'the installer')
    return parser

  def Run(self, context):
    targets = cr.Target.GetTargets(context)
    if not cr.Installer.Skipping(context):
      cr.Builder.Build(context, targets, [])
    cr.Installer.Reinstall(context, targets, context.remains)
