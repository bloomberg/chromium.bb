# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""CLI for running Chrome OS tests from lib/cros_test.py."""

from __future__ import print_function

from chromite.lib import cros_test

def main(argv):
  opts = cros_test.ParseCommandLine(argv)
  opts.Freeze()
  return cros_test.CrOSTest(opts).Run()
