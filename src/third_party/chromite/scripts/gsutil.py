# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper around gsutil.

This takes care of downloading the pinned version we use in chromite.
"""

from __future__ import print_function

import os

from chromite.lib import gs


def main(argv):
  gsutil = gs.GSContext.GetDefaultGSUtilBin()
  os.execv(gsutil, ['gsutil'] + argv)
