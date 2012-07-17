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
import dgen_add_patterns
import dgen_decoder_output
import dgen_test_output

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
    tables = None

    # Define default command line arguments.
    cl_args = {'add-rule-patterns': 'True'}

    # Strip off remaining command line arguments and add.
    remaining_args = argv[4:]
    while remaining_args and remaining_args[0].startswith('--'):
      arg = remaining_args[0][len('--'):]
      remaining_args.pop(0)
      index = arg.find('=')
      if index == -1:
        cl_args[arg] = 'True'
      else:
        cl_args[arg[0:index]] = arg[index+1:]

    print "cl args = %s" % cl_args

    # Strip off tables.
    if remaining_args:
      tables = remaining_args

    print "Decoder Generator reading ", table_filename
    f = open(table_filename, 'r')
    decoder = dgen_input.parse_tables(f)
    if cl_args.get('add-rule-patterns') == 'True':
      decoder = dgen_add_patterns.add_rule_pattern_constraints(decoder)
    f.close()

    print "Successful - got %d tables." % len(decoder.tables())

    print "Generating %s..." % output_filename
    f = open(output_filename, 'w')

    if output_filename.endswith('tests.cc'):
      dgen_test_output.generate_tests_cc(decoder,
                                         decoder_name,
                                         f, cl_args, tables)
    elif output_filename.endswith('named_classes.h'):
      dgen_test_output.generate_named_classes_h(
          decoder, decoder_name, _localize_filename(output_filename),
          f, cl_args)
    elif output_filename.endswith('named_decoder.h'):
      dgen_test_output.generate_named_decoder_h(
          decoder, decoder_name, _localize_filename(output_filename),
          f, cl_args)
    elif output_filename.endswith('.h'):
      dgen_decoder_output.generate_h(
          decoder, decoder_name, _localize_filename(output_filename),
          f, cl_args)
    elif output_filename.endswith('named.cc'):
      dgen_test_output.generate_named_cc(
          decoder, decoder_name, _localize_filename(output_filename),
          f, cl_args)
    elif output_filename.endswith('.cc'):
      dgen_decoder_output.generate_cc(
          decoder, decoder_name, _localize_filename(output_filename),
          f, cl_args)
    else:
      print 'Error: output filename not of form "*.{h,cc}"'
    f.close()
    print "Completed."

    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
