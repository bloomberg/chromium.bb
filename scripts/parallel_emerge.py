# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Thin wrapper to dispatch to old or new parallel_emerge implementation."""

from __future__ import print_function

import os

from chromite.lib import cros_logging
from chromite.scripts import parallel_emerge_new as NEW_WRAPPER

_USE_NEW = os.environ.get('USE_NEW_PARALLEL_EMERGE', '1') == '1'

# parallel_emerge_old.py runs code at import time, so only import it if we
# intend to use it. This avoids getting a mix of the old and new behaviors.
if not _USE_NEW:
  from chromite.scripts import parallel_emerge_old as PARALLEL_EMERGE_OLD


def main(argv):
  if _USE_NEW:
    cros_logging.notice('Using new parallel_emerge implementation.'
                        ' Please report any issues at crbug.com/989962')
    NEW_WRAPPER.main(argv)
  else:
    PARALLEL_EMERGE_OLD.main(argv)
