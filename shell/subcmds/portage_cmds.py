# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implementation of the various portage wrapper commands."""


from chromite.shell import subcmd


class EbuildCmd(subcmd.WrappedChrootCmd):
  """Run ebuild."""

  def __init__(self):
    """EbuildCmd constructor."""
    # Just call the WrappedChrootCmd superclass, which does most of the work.
    super(EbuildCmd, self).__init__(
        ['ebuild-%s'], ['ebuild'],
        need_args=True,
        env_whitelist=['CHROME_ORIGIN', 'FEATURES', 'USE']
    )


class EmergeCmd(subcmd.WrappedChrootCmd):
  """Run emerge."""

  def __init__(self):
    """EmergeCmd constructor."""
    # Just call the WrappedChrootCmd superclass, which does most of the work.
    super(EmergeCmd, self).__init__(
        ['emerge-%s'], ['sudo', 'emerge'],
        need_args=True,
        env_whitelist=['CHROME_ORIGIN', 'FEATURES', 'USE']
    )


class EqueryCmd(subcmd.WrappedChrootCmd):
  """Run equery."""

  def __init__(self):
    """EqueryCmd constructor."""
    # Just call the WrappedChrootCmd superclass, which does most of the work.
    super(EqueryCmd, self).__init__(
        ['equery-%s'], ['equery'],
        need_args=True
    )


class PortageqCmd(subcmd.WrappedChrootCmd):
  """Run portageq."""

  def __init__(self):
    """PortageqCmd constructor."""
    # Just call the WrappedChrootCmd superclass, which does most of the work.
    super(PortageqCmd, self).__init__(
        ['portageq-%s'], ['portageq'],
        need_args=True
    )
