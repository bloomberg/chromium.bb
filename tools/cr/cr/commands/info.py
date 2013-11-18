# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the info implementation of Command."""

import cr


class InfoCommand(cr.Command):
  """The cr info command implementation."""

  def __init__(self):
    super(InfoCommand, self).__init__()
    self.help = 'Print information about the cr environment'

  def AddArguments(self, subparsers):
    parser = super(InfoCommand, self).AddArguments(subparsers)
    parser.add_argument(
        '-s', '--short', dest='_short',
        action='store_true', default=False,
        help='Short form results, useful for scripting.'
    )
    self.ConsumeArgs(parser, 'the environment')
    return parser

  def EarlyArgProcessing(self, context):
    if getattr(context.args, '_short', False):
      self.requires_build_dir = False

  def Run(self, context):
    if context.remains:
      for var in context.remains:
        if getattr(context.args, '_short', False):
          val = context.Find(var)
          if val is None:
            val = ''
          print val
        else:
          print var, '=', context.Find(var)
    else:
      cr.base.client.PrintInfo(context)

