# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run xz from PATH with a thread for each core in the system."""

from __future__ import print_function

import os
import sys


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


def main(argv):
  os.execvp('xz', ['xz', '-T0'] + argv)
