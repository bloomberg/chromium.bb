# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromite base class.

This module maintains information about the build environemnt, including
paths and defaults. It includes methods for querying the environemnt.

(For now this is just a placeholder to illustrate the concept)
"""

from chromite.lib import operation


class ChromiteError(Exception):
  """A Chromite exception, such as a build error or missing file.

  We define this as an exception type so that we can report proper
  meaningful errors to upper layers rather than just the OS 'File not
  found' low level message.
  """
  pass


class ChromiteEnv:
  """Chromite environment class.

  This holds information about a Chromite environment, including its
  chroot, builds, images and so on. It is intended to understand the paths
  to use for each object, and provide methods to accessing and querying
  the various things in the environment.
  """
  def __init__(self):
    # We have at least a single overall operation always, so set it up.
    self._oper = operation.Operation('operation')

  def __del__(self):
    del self._oper

  def GetOperation(self):
    """Returns the current operation in progress
    """
    return self._oper
