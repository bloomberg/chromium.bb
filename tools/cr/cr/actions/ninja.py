# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module to add ninja support to cr."""

import os

import cr

_PHONY_SUFFIX = ': phony'
_LINK_SUFFIX = ': link'


class NinjaBuilder(cr.Builder):
  """An implementation of Builder that uses ninja to do the actual build."""

  # Some basic configuration installed if we are enabled.
  ENABLED = cr.Config.From(
      NINJA_BINARY=os.path.join('{DEPOT_TOOLS}', 'ninja'),
      NINJA_JOBS=200,
      NINJA_PROCESSORS=12,
      NINJA_BUILD_FILE=os.path.join('{CR_BUILD_DIR}', 'build.ninja'),
      GOMA_DIR=os.path.join('{GOOGLE_CODE}', 'goma'),
      # Don't rename to GOMA_* or Goma will complain: "unkown GOMA_ parameter".
      NINJA_GOMA_CTL=os.path.join('{GOMA_DIR}', 'goma_ctl.py'),
      NINJA_GOMA_CC=os.path.join('{GOMA_DIR}', 'gomacc'),
      NINJA_GOMA_LINE='cc = {NINJA_GOMA_CC} $',
  )
  # A placeholder for the system detected configuration
  DETECTED = cr.Config('DETECTED')

  def __init__(self):
    super(NinjaBuilder, self).__init__()
    self._targets = []

  def Build(self, context, targets, arguments):
    # Make sure Goma is started if Ninja is set to use it.
    # This may be redundant, but it currently improves reliability.
    try:
      with open(context.Get('NINJA_BUILD_FILE'), 'r') as f:
        if f.readline().rstrip('\n') == context.Get('NINJA_GOMA_LINE'):
          # Goma is active, so make sure it's started.
          cr.Host.ExecuteSilently(
              context,
              '{NINJA_GOMA_CTL}',
              'ensure_start'
          )
    except IOError:
      pass

    build_arguments = [target.build_target for target in targets]
    build_arguments.extend(arguments)
    cr.Host.Execute(
        context,
        '{NINJA_BINARY}',
        '-C{CR_BUILD_DIR}',
        '-j{NINJA_JOBS}',
        '-l{NINJA_PROCESSORS}',
        *build_arguments
    )

  def Clean(self, context, targets, arguments):
    build_arguments = [target.build_target for target in targets]
    build_arguments.extend(arguments)
    cr.Host.Execute(
        context,
        '{NINJA_BINARY}',
        '-C{CR_BUILD_DIR}',
        '-tclean',
        *build_arguments
    )

  def GetTargets(self, context):
    """Overridden from Builder.GetTargets."""
    if not self._targets:
      try:
        context.Get('CR_BUILD_DIR', raise_errors=True)
      except KeyError:
        return self._targets
      output = cr.Host.Capture(
          context,
          '{NINJA_BINARY}',
          '-C{CR_BUILD_DIR}',
          '-ttargets',
          'all'
      )
      for line in output.split('\n'):
        line = line.strip()
        if line.endswith(_PHONY_SUFFIX):
          target = line[:-len(_PHONY_SUFFIX)].strip()
          self._targets.append(target)
        elif line.endswith(_LINK_SUFFIX):
          target = line[:-len(_LINK_SUFFIX)].strip()
          self._targets.append(target)
    return self._targets

  @classmethod
  def ClassInit(cls):
    # TODO(iancottrell): If we can't detect ninja, we should be disabled.
    ninja_binaries = cr.Host.SearchPath('ninja')
    if ninja_binaries:
      cls.DETECTED.Set(NINJA_BINARY=ninja_binaries[0])
