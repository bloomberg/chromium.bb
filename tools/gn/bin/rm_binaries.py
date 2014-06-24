# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script removes the platform-specific GN binaries from tools/gn/bin* now
# that the binaries have been moved to src/buildtools. Keeping the old ones
# around will be pretty confusing since the version will be out-of-date.
#
# This script assumes it is located in tools/gn/bin and locates the binaries
# relative to itself.
#
# TODO(brettw) remove this script when people have likely been updated.
# End of July 2014 would be good.

import os
import sys

bin_dir = os.path.dirname(__file__)

# Typically only one platform will have binaries. Remove them all just in case,
# but ignore all errors.

try:
  os.remove(os.path.join(bin_dir, "linux", "gn"))
except:
  pass

try:
  os.remove(os.path.join(bin_dir, "linux", "gn32"))
except:
  pass

try:
  os.remove(os.path.join(bin_dir, "mac", "gn"))
except:
  pass

try:
  os.remove(os.path.join(bin_dir, "win", "gn.exe"))
except:
  pass

