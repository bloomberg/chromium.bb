#!/usr/bin/python
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""ChromiteCmd abstract class and related functions."""


class ChromiteCmd(object):
  """The abstract base class of commands listed at the top level of chromite."""

  def __init__(self):
    """ChromiteCmd constructor."""
    super(ChromiteCmd, self).__init__()


  def Run(self, raw_argv, chroot_config=None):
    """Run the command.

    All subclasses must implement this.

    Args:
      raw_argv: Command line arguments, including this command's name, but not
          the chromite command name or chromite options.
      chroot_config: A SafeConfigParser for the chroot config; or None chromite
          was called from within the chroot.
    """
    # Must be implemented by subclass...
    raise NotImplementedError()
