# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper around gsutil.

This takes care of downloading the pinned version we use in chromite.
"""

from __future__ import print_function

import os
import sys

from chromite.lib import gs


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


def main(argv):
  gsutil = gs.GSContext.GetDefaultGSUtilBin()
  os.execv(gsutil, ['gsutil'] + argv)
