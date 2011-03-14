# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implementation of the 'workon' chromite command."""


import chromite.lib.cros_build_lib as cros_lib
from chromite.shell import subcmd


class WorkonCmd(subcmd.WrappedChrootCmd):
  """Run cros_workon."""

  def __init__(self):
    """WorkonCmd constructor."""
    # Just call the WrappedChrootCmd superclass, which does most of the work.
    super(WorkonCmd, self).__init__(
	'WORKON',
        ['cros_workon-%s'],
        ['./cros_workon', '--host'],
        need_args=True
    )

  def Run(self, raw_argv, *args, **kwargs):
    """Run the command.

    We do just a slight optimization to help users with a common typo.

    Args:
      raw_argv: Command line arguments, including this command's name, but not
          the chromite command name or chromite options.
      args: The rest of the positional arguments.  See _DoWrappedChrootCommand.
      kwargs: The keyword arguments.  See _DoWrappedChrootCommand.
    """
    # Slight optimization, just since I do this all the time...
    if len(raw_argv) >= 2:
      if raw_argv[1] in ('start', 'stop', 'list', 'list-all', 'iterate'):
        cros_lib.Warning('OOPS, looks like you forgot a board name. Pick one.')
        raw_argv = raw_argv[:1] + [''] + raw_argv[1:]

    super(WorkonCmd, self).Run(raw_argv, *args, **kwargs)
