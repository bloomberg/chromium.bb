# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the Debugger base class."""

import cr


class Debugger(cr.Action, cr.Plugin.Type):
  """Base class for implementing debuggers.

  Implementations must override the Invoke and Attach methods.
  """

  SELECTOR_ARG = '--debugger'
  SELECTOR = 'CR_DEBUGGER'
  SELECTOR_HELP = 'Sets the debugger to use for debug commands.'

  @classmethod
  def AddArguments(cls, command, parser):
    cr.Runner.AddSelectorArg(command, parser)

  @classmethod
  def ShouldInvoke(cls, context):
    """Checks if the debugger is attaching or launching."""
    return not cr.Runner.Skipping(context)

  @cr.Plugin.activemethod
  def Restart(self, context, targets, arguments):
    """Ask the debugger to restart.

    Defaults to a Kill Invoke sequence.
    """
    self.Kill(context, targets, [])
    self.Invoke(context, targets, arguments)

  @cr.Plugin.activemethod
  def Kill(self, context, targets, arguments):
    """Kill the running debugger."""
    cr.Runner.Kill(context, targets, arguments)

  @cr.Plugin.activemethod
  def Invoke(self, context, targets, arguments):
    """Invoke the program within a debugger."""
    raise NotImplementedError('Must be overridden.')

  @cr.Plugin.activemethod
  def Attach(self, context, targets, arguments):
    """Attach a debugger to a running program."""
    raise NotImplementedError('Must be overridden.')
