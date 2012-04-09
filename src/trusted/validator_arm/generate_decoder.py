#!/usr/bin/python
#
# Copyright 2012 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
#

"""Decoder Generator script.

Usage: generate-decoder.py <table-file> <output-cc-file> <decoder-name>

Note: If the file ends with named.{h,cc}, it is assumed that one should
build the corresponding source file for named classes, so that testing
is easier to perform. In either case, the .h file will declare a decoder
state (with the given decoder name) to decode instructions, while the
.cc file will define the methods for the declared decoder state.
"""

import re
import sys
import dgen_input
import dgen_output

def _localize_filename(filename):
  """ Strips off directories above 'native_client', returning
      a location neutral name for the file
  """
  m = re.match(r'.*/(native_client/.*)', filename)
  if m:
    return m.group(1)
  else:
    # Don't know localized
    return filename

def main(argv):
    table_filename = argv[1]
    output_filename = argv[2]
    decoder_name = argv[3]

    print "Decoder Generator reading ", table_filename
    f = open(table_filename, 'r')
    tables = dgen_input.parse_tables(f)
    f.close()

    print "Successful - got %d tables." % len(tables)

    print "Generating %s..." % output_filename
    f = open(output_filename, 'w')

    if output_filename.endswith('named.h'):
      dgen_output.generate_decoder_h(tables,
                                     decoder_name,
                                     _localize_filename(output_filename),
                                     True,
                                     dgen_output.COutput(f))
    elif output_filename.endswith('.h'):
      dgen_output.generate_decoder_h(tables,
                                     decoder_name,
                                     _localize_filename(output_filename),
                                     False,
                                     dgen_output.COutput(f))
    elif output_filename.endswith('named.cc'):
      dgen_output.generate_decoder_cc(tables,
                                      decoder_name,
                                      _localize_filename(output_filename),
                                      True,
                                      dgen_output.COutput(f))
    elif output_filename.endswith('.cc'):
      dgen_output.generate_decoder_cc(tables,
                                      decoder_name,
                                      _localize_filename(output_filename),
                                      False,
                                      dgen_output.COutput(f))
    else:
      print 'Error: output filename not of form "*.{h,cc}"'
    f.close()
    print "Completed."

    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
