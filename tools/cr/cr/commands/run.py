# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the run command."""

import cr


class RunCommand(cr.Command):
  """The implementation of the run command.

  This first uses Builder to bring the target up to date.
  It then uses Installer to install the target (if needed), and
  finally it uses Runner to run the target.
  You can use skip version to not perform any of these steps.
  """

  def __init__(self):
    super(RunCommand, self).__init__()
    self.help = 'Invoke a target'

  def AddArguments(self, subparsers):
    parser = super(RunCommand, self).AddArguments(subparsers)
    cr.Builder.AddArguments(self, parser)
    cr.Installer.AddArguments(self, parser)
    cr.Runner.AddArguments(self, parser)
    cr.Target.AddArguments(self, parser, allow_multiple=True)
    self.ConsumeArgs(parser, 'the binary')
    return parser

  def Run(self, context):
    targets = cr.Target.GetTargets(context)
    test_targets = [target for target in targets if target.is_test]
    run_targets = [target for target in targets if not target.is_test]
    if cr.Installer.Skipping(context):
      # No installer, only build test targets
      build_targets = test_targets
    else:
      build_targets = targets
    if build_targets:
      cr.Builder.Build(context, build_targets, [])
    # See if we can use restart when not installing
    if cr.Installer.Skipping(context):
      cr.Runner.Restart(context, targets, context.remains)
    else:
      cr.Runner.Kill(context, run_targets, [])
      cr.Installer.Reinstall(context, run_targets, [])
      cr.Runner.Invoke(context, targets, context.remains)

