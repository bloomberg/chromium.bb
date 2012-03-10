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

import driver_tools
from driver_env import env
from driver_log import Log

def main(argv):
  f = driver_tools.DriverOpen(argv[0], 'rb')
  data = f.read()
  f.close()

  print "Meta file has length %d" % len(data)
  while len(data) > 0:
    magic = LSBInt(data[0:4])
    if magic != 0xfadefade:
      print "bad magic"
      return 1
    size = LSBInt(data[4:8])
    filename = data[8:8+64].strip('\0')
    filedata = data[8+64:8+64+size]
    out_filename = filename + '.stub'
    print "Writing %s (%d bytes)" % (out_filename, size)
    fout = driver_tools.DriverOpen(out_filename, 'wb')
    fout.write(filedata)
    fout.close()
    offset = 8+64+size
    offset = 8*((offset+7)/8)
    data = data[offset:]
  return 0

# Convert 4 bytes to an integer (LSB byte order)
def LSBInt(b):
  r = ord(b[3])
  r *= 256
  r += ord(b[2])
  r *= 256
  r += ord(b[1])
  r *= 256
  r += ord(b[0])
  return r
