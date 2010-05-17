#!/usr/bin/python

# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import unittest

# This is a shortcut for running unittest on the tests from the
# modules below.  This assumes that the class names in the modules
# don't collide.
from btarget_test import *
from dirtree_test import *

if __name__ == "__main__":
  unittest.main()
