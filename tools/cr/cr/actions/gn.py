# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module to add gn support to cr."""

import cr
import shlex
import os

GN_ARG_PREFIX = 'GN_ARG_'


class GnPrepareOut(cr.PrepareOut):
  """A prepare action that runs gn whenever you select an output directory."""

  ACTIVE = cr.Config.From(
      GN_ARG_is_component_build='true',
  )

  @property
  def enabled(self):
    # Disabled on Android for now.
    return not cr.AndroidPlatform.GetInstance().is_active

  def UpdateContext(self):
    # Collapse GN_ARGS from all GN_ARG prefixes
    gn_args = cr.context.Find('GN_ARGS') or ''
    for key, value in cr.context.exported.items():
      if key.startswith(GN_ARG_PREFIX):
        gn_args += ' %s=%s' % (key[len(GN_ARG_PREFIX):], value)

    gn_args += (' is_debug=%s' %
        'true' if cr.context['CR_BUILDTYPE'] == 'Debug' else 'false')

    cr.context['GN_ARGS'] = gn_args.strip()
    if cr.context.verbose >= 1:
      print cr.context.Substitute('GN_ARGS = {GN_ARGS}')

  def Prepare(self):
    if cr.context.verbose >= 1:
      print cr.context.Substitute('Invoking gn with {GN_ARGS}')

    args_file = os.path.join(cr.context['CR_SRC'],
                             cr.context['CR_OUT_FULL'],
                             'args.gn')
    args = {}
    for arg in shlex.split(cr.context['GN_ARGS']):
      key, value = arg.split('=', 1)
      args[key] = value

    # Override any existing settings.
    arg_lines = []
    if os.path.exists(args_file):
      with open(args_file) as f:
        for line in f:
          key = line.split('=', 1)[0].strip()
          if key not in args:
            arg_lines.append(line.strip())

    # Append new settings.
    for key, value in args.items():
      arg_lines.append('%s = %s' % (key, value))

    with open(args_file, 'w') as f:
      f.write('\n'.join(arg_lines) + '\n')

    cr.Host.Execute(
        'gn',
        'gen',
        '{CR_OUT_FULL}',
    )
