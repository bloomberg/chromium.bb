# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the run command."""

import cr


class DebugCommand(cr.Command):
  """The implementation of the debug command.

  This is much like the run command except it launches the program under
  a debugger instead.
  """

  def __init__(self):
    super(DebugCommand, self).__init__()
    self.help = 'Debug a binary'

  def AddArguments(self, subparsers):
    parser = super(DebugCommand, self).AddArguments(subparsers)
    cr.Builder.AddArguments(self, parser)
    cr.Installer.AddArguments(self, parser)
    cr.Debugger.AddArguments(self, parser)
    cr.Target.AddArguments(self, parser)
    self.ConsumeArgs(parser, 'the binary')
    return parser

  def Run(self, context):
    targets = cr.Target.GetTargets(context)
    if not cr.Debugger.ShouldInvoke(context):
      cr.Debugger.Attach(context, targets, context.remains)
    elif cr.Installer.Skipping(context):
      cr.Debugger.Restart(context, targets, context.remains)
    else:
      cr.Builder.Build(context, targets, [])
      cr.Debugger.Kill(context, targets, [])
      cr.Installer.Reinstall(context, targets, [])
      cr.Debugger.Invoke(context, targets, context.remains)
