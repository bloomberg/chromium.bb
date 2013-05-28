# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import itertools


def SkipHeader(objdump_output_lines):
  # Objdump prints few lines about file and section before disassembly
  # listing starts, so we have to skip that.
  objdump_header_size = 7
  return itertools.islice(objdump_output_lines, objdump_header_size, None)


ObjdumpLine = collections.namedtuple('ObjdumpLine',
                                     ['address', 'bytes', 'command'])


def ParseLine(line):
  """Split objdump line.

  Typical line from objdump output looks like this:

  "   0:  90                    nop"

  There are three columns (separated with tabs):
    address: offset for a particular line
    bytes: few bytes which encode a given command
    command: textual representation of command

  Args:
      line: objdump line (a single one).
  Returns
      ObjdumpLine tuple.
  """

  address, bytes, command = line.strip().split('\t')
  assert command.strip() != ''

  return ObjdumpLine(address, bytes.split(), command)
