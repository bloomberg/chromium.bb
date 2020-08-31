# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Hook to stop people from running `git cl`."""

from __future__ import print_function

import sys


def CheckChangeOnUpload(_input_api, _output_api):
  print('ERROR: CrOS repos use `repo upload`, not `git cl upload`.',
        file=sys.stderr)
  sys.exit(1)
