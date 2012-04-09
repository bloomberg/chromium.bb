#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  pnacl/build.sh driver
# in order for them to take effect in the scons build.  This command
# updates the copy in the toolchain/ tree.
#

# __import__ is required because of the dash in the filename.
# TODO(pdox): Change - to _ in python file names.
pnacl_driver = __import__("pnacl-driver")

def main(argv):
  pnacl_driver.env.set('IS_CXX', '1')
  return pnacl_driver.main(argv)

def get_help(argv):
  pnacl_driver.env.set('IS_CXX', '1')
  return pnacl_driver.get_help(argv)
