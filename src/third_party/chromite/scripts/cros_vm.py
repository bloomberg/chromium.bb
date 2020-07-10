# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""CLI entry point into lib/vm.py; used for VM management."""

from __future__ import print_function

from chromite.lib import vm

def main(argv):
  opts = vm.VM.GetParser().parse_args(argv)
  opts.Freeze()

  vm.VM(opts).Run()
