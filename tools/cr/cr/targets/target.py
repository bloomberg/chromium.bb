# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module to hold the Target plugin."""

import operator
import re

import cr

DEFAULT = cr.Config.From(
    CR_DEFAULT_TARGET='chrome',
)


class Target(cr.Config, cr.AutoExport):
  """Base class for implementing cr targets.

  A target is something that can be built and run.
  """

  # The default base priority
  PRIORITY = 0
  # The default pattern used to try to detect whether a target is a test and
  # should use the test runner.
  TEST_PATTERN = re.compile('tests?$')
  # The special "test type" that means it's not a test.
  NOT_A_TEST = 'no'
  # The default choice for the type of test when it can't be determined.
  NORMAL_TEST = 'gtest'
  # TODO(iancottrell): support the other test types
  TEST_TYPES = [NOT_A_TEST, NORMAL_TEST]

  def  __init__(self, context, target_name):
    super(Target, self).__init__(target_name)
    self.context = context
    test_type = None
    if self.TEST_PATTERN.search(target_name):
      test_type = self.NORMAL_TEST
    config = cr.Config('DEFAULTS').From(
        CR_TARGET=target_name,
        CR_TARGET_NAME='{CR_TARGET}',
        CR_BUILD_TARGET=cr.Config.Optional(
            '{CR_TARGET}{CR_TARGET_SUFFIX}', '{CR_TARGET}'),
        CR_RUN_ARGUMENTS='',
        CR_TEST_TYPE=test_type,
    )
    self.AddChildren(config, context)
    if hasattr(self, 'CONFIG'):
      self.AddChild(self.CONFIG)
    if not self.valid:
      self.Set(CR_TARGET_SUFFIX='')
    self.test_type = self.Find('CR_TEST_TYPE')
    self.target_name = self.Find('CR_TARGET_NAME')

  @property
  def build_target(self):
    return self.Get('CR_BUILD_TARGET')

  @property
  def verbose(self):
    return self.context.verbose

  @property
  def dry_run(self):
    return self.context.dry_run

  @property
  def valid(self):
    return cr.Builder.IsTarget(self.context, self.build_target)

  @property
  def is_test(self):
    return self.test_type and self.test_type != self.NOT_A_TEST

  @classmethod
  def AddArguments(cls, command, parser, allow_multiple=False):
    nargs = '?'
    help_string = 'The target to {0}'
    if allow_multiple:
      nargs = '*'
      help_string = 'The target(s) to {0}'
    parser.add_argument(
        '_targets', metavar='target',
        help=help_string.format(command.name),
        nargs=nargs
    )

  @classmethod
  def AllTargets(cls):
    yield cls
    for child in cls.__subclasses__():
      for t in child.AllTargets():
        yield t

  @classmethod
  def CreateTarget(cls, context, target_name):
    """Attempts to build a target by name.

    This searches the set of installed targets in priority order to see if any
    of them are willing to handle the supplied name.
    If a target cannot be found, the program will be aborted.
    Args:
      context: The context to run in.
      target_name: The name of the target we are searching for.
    Returns:
      The target that matched.
    """
    target_clses = sorted(
        cls.AllTargets(),
        key=operator.attrgetter('PRIORITY'),
        reverse=True
    )
    for handler in target_clses:
      target = handler.Build(context, target_name)
      if target:
        if not target.valid:
          print 'Invalid target {0} as {1}'.format(
              target_name, target.build_target)
          exit(1)
        return target
    print 'Unknown target {0}'.format(target_name)
    exit(1)

  @classmethod
  def GetTargets(cls, context):
    target_names = getattr(context.args, '_targets', None)
    if not target_names:
      target_names = [context.Get('CR_DEFAULT_TARGET')]
    elif hasattr(target_names, 'swapcase'):
      # deal with the single target case
      target_names = [target_names]
    return [cls.CreateTarget(context, target_name)
            for target_name in target_names]

  @classmethod
  def Build(cls, context, target_name):
    return cls(context, target_name)


class NamedTarget(Target):
  """A base class for explicit named targets.

  Only matches a target if the name is an exact match.
  Up it's priority to come ahead of general purpose rule matches.
  """
  NAME = None
  PRIORITY = Target.PRIORITY + 1

  @classmethod
  def Build(cls, context, target_name):
    try:
      if target_name == cls.NAME:
        return cls(context, target_name)
    except AttributeError:
      pass
    return None
