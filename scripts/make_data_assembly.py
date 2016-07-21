#!/usr/bin/python2

# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import binascii
import sys

if len(sys.argv) < 2:
  sys.exit("Usage: %s icu_data_file [output_assembly_file]" % sys.argv[0])

input_file = sys.argv[1]
n = input_file.find(".dat")
if n == -1:
  sys.exit("%s is not an ICU .dat file." % input_file)

if len(sys.argv) < 3:
  output_file = input_file[0:n] + "_dat.S"
else:
  output_file = sys.argv[2]

if input_file.find("l.dat") == -1:
  if input_file.find("b.dat") == -1:
    sys.exit("%s has no endianness marker." % input_file)
  else:
    step = 1
else:
  step = -1

input_data = open(input_file, 'rb').read()
n = input_data.find("icudt")
if n == -1:
  sys.exit("Cannot find a version number in %s." % input_file)

version_number = input_data[n + 5:n + 7]

output = open(output_file, 'w')
output.write(".globl icudt" + version_number + "_dat\n"
             "\t.section .note.GNU-stack,\"\",%progbits\n"
             "\t.section .rodata\n"
             "\t.balign 16\n"
             "#ifdef U_HIDE_DATA_SYMBOL\n"
             "\t.hidden icudt" + version_number + "_dat\n"
             "#endif\n"
             "\t.type icudt" + version_number + "_dat,%object\n"
             "icudt" + version_number + "_dat:\n")

split = [binascii.hexlify(input_data[i:i + 4][::step]).upper().lstrip('0')
        for i in range(0, len(input_data), 4)]

for i in range(len(split)):
  if (len(split[i]) == 0):
    value = '0'
  elif (len(split[i]) == 1):
    if not any((c in '123456789') for c in split[i]):
      value = '0x0' + split[i]
    else:
      value = split[i]
  elif (len(split[i]) % 2 == 1):
    value = '0x0' + split[i]
  else:
    value = '0x' + split[i]

  if (i % 32 == 0):
    output.write("\n.long ")
  else:
    output.write(",")
  output.write(value)

output.write("\n")
output.close()
print "Generated " + output_file
