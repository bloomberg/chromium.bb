#!/usr/bin/python

# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import os
import sys

import action_tree
import build


# This is experimental and will probably not work fully.
# It tries to install each module into a separate prefix dir,
# each of which has to be put onto PATH (and similar env vars).
# Not all modules support this easily.

def Main(args):
  base_dir = os.getcwd()
  top = build.AllMods(base_dir, use_shared_prefix=False)
  action_tree.action_main(top, args)


if __name__ == "__main__":
  Main(sys.argv[1:])
