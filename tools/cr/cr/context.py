# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Application context management for the cr tool.

Contains all the support code to enable the shared context used by the cr tool.
This includes the configuration variables and command line handling.
"""

import argparse
import os
import cr


class _DumpVisitor(cr.visitor.ExportVisitor):
  """A visitor that prints all variables in a config hierarchy."""

  def __init__(self, with_source):
    super(_DumpVisitor, self).__init__({})
    self.to_dump = {}
    self.with_source = with_source

  def StartNode(self):
    if self.with_source:
      self._DumpNow()
    super(_DumpVisitor, self).StartNode()

  def EndNode(self):
    if self.with_source or not self.stack:
      self._DumpNow()
    super(_DumpVisitor, self).EndNode()
    if not self.stack:
      self._DumpNow()

  def Visit(self, key, value):
    super(_DumpVisitor, self).Visit(key, value)
    if key in self.store:
      str_value = str(self.store[key])
      if str_value != str(os.environ.get(key, None)):
        self.to_dump[key] = str_value

  def _DumpNow(self):
    if self.to_dump:
      if self.with_source:
        print 'From', self.Where()
      for key in sorted(self.to_dump.keys()):
        print '  ', key, '=', self.to_dump[key]
      self.to_dump = {}


class _ShowHelp(argparse.Action):
  """An argparse action to print the help text.

  This is like the built in help text printing action, except it knows to do
  nothing when we are just doing the early speculative parse of the args.
  """

  def __call__(self, parser, namespace, values, option_string=None):
    if parser.context.speculative:
      return
    command = cr.Command.GetActivePlugin(parser.context)
    if command:
      command.parser.print_help()
    else:
      parser.print_help()
    exit(1)


class _ArgumentParser(argparse.ArgumentParser):
  """An extension of an ArgumentParser to enable speculative parsing.

  It supports doing an early parse that never produces errors or output, to do
  early collection of arguments that may affect what other arguments are
  allowed.
  """

  def error(self, message):
    if self.context.speculative:
      return
    super(_ArgumentParser, self).error(message)

  def parse_args(self):
    if self.context.speculative:
      result = self.parse_known_args()
      if result:
        return result[0]
      return None
    return super(_ArgumentParser, self).parse_args()

  def parse_known_args(self, args=None, namespace=None):
    result = super(_ArgumentParser, self).parse_known_args(args, namespace)
    if result is None:
      return namespace, None
    return result


class Context(cr.config.Config, cr.loader.AutoExport):
  """The base context holder for the cr system.

  This is passed to almost all methods in the cr system, and holds the common
  context they all shared. Mostly this is stored in the Config structure of
  variables.
  """

  def __init__(self, description='', epilog=''):
    super(Context, self).__init__('Context')
    self._args = None
    self._arguments = cr.config.Config('ARGS')
    self._derived = cr.config.Config('DERIVED')
    self.AddChildren(*cr.config.GLOBALS)
    self.AddChildren(
        cr.config.Config('ENVIRONMENT', literal=True, export=True).Set(
            {k: self.ParseValue(v) for k, v in os.environ.items()}),
        self._arguments,
        self._derived,
    )
    # Build the command line argument parser
    self._parser = _ArgumentParser(add_help=False, description=description,
                                   epilog=epilog)
    self._parser.context = self
    self._subparsers = self.parser.add_subparsers()
    # Add the global arguments
    self.AddCommonArguments(self._parser)
    self._gclient = {}
    # Try to detect the current client information
    cr.base.client.DetectClient(self)

  def AddSubParser(self, source):
    parser = source.AddArguments(self._subparsers)
    parser.context = self

  @classmethod
  def AddCommonArguments(cls, parser):
    """Adds the command line arguments common to all commands in cr."""
    parser.add_argument(
        '-h', '--help',
        action=_ShowHelp, nargs=0,
        help='show the help message and exit.'
    )
    parser.add_argument(
        '--dry-run', dest='CR_DRY_RUN',
        action='store_true', default=None,
        help="""
          Don't execute commands, just print them. Implies verbose.
          Overrides CR_DRY_RUN
          """
    )
    parser.add_argument(
        '-v', '--verbose', dest='CR_VERBOSE',
        action='count', default=None,
        help="""
          Print information about commands being performed.
          Repeating multiple times increases the verbosity level.
          Overrides CR_VERBOSE
          """
    )

  @property
  def args(self):
    return self._args

  @property
  def arguments(self):
    return self._arguments

  @property
  def speculative(self):
    return self._speculative

  @property
  def derived(self):
    return self._derived

  @property
  def parser(self):
    return self._parser

  @property
  def remains(self):
    remains = getattr(self._args, '_remains', None)
    if remains and remains[0] == '--':
      remains = remains[1:]
    return remains

  @property
  def verbose(self):
    if self.autocompleting:
      return False
    return self.Find('CR_VERBOSE') or self.dry_run

  @property
  def dry_run(self):
    if self.autocompleting:
      return True
    return self.Find('CR_DRY_RUN')

  @property
  def autocompleting(self):
    return 'COMP_WORD' in os.environ

  @property
  def gclient(self):
    if not self._gclient:
      self._gclient = cr.base.client.ReadGClient(self)
    return self._gclient

  def WriteGClient(self):
    if self._gclient:
      cr.base.client.WriteGClient(self)

  def ParseArgs(self, speculative=False):
    cr.plugin.DynamicChoices.only_active = not speculative
    self._speculative = speculative
    self._args = self._parser.parse_args()
    self._arguments.Wipe()
    if self._args:
      self._arguments.Set(
          {k: v for k, v in vars(self._args).items() if v is not None})

  def DumpValues(self, with_source):
    _DumpVisitor(with_source).VisitNode(self)

