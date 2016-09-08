# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""For running module as script."""

from __future__ import print_function

import sys

from chromite.lib import cros_logging as logging
from chromite.scripts import sysmon

try:
  sysmon.main(sys.argv[1:])
except Exception:
  logging.exception('sysmon throws an error')
  raise
