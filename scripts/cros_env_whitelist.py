# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Print the environment whitelist.
"""

from chromite.buildbot import constants


def main(_argv):
  print ' '.join(constants.CHROOT_ENVIRONMENT_WHITELIST)
