#!/usr/bin/python
#
# Copyright 2012 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
#

"""Decoder Generator script.

Usage: generate-decoder.py <table-file> <output-cc-file> <decoder-name> \
       <options>

Options include:

  --add_rule_patterns=bool - If bool='True', rule patterns are added (default).
        If =bool is omitted, 'True' is assumped. Legal values for bool is
        'True' and 'False'.
  --table_remove=name - Remove table 'name' from decoder. May be repeated.

  name - Only generate tests for table 'name'. May be repeated.

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
import dgen_actuals

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
    cl_args = {'add-rule-patterns': 'True',
               'table_remove': [],
               'table': [],
               }

    # Strip off remaining command line arguments and add.
    remaining_args = argv[4:]
    while remaining_args and remaining_args[0].startswith('--'):
      arg = remaining_args[0][len('--'):]
      remaining_args.pop(0)
      index = arg.find('=')
      if index == -1:
        cl_args[arg] = 'True'
      else:
        cl_name = arg[0:index]
        cl_value = arg[index+1:]
        if (cl_name in cl_args.keys() and
            isinstance(cl_args[cl_name], list)):
          # Repeatable CL argument, add value to list.
          cl_args[cl_name] = cl_args[cl_name] + [cl_value]
        else:
          # Single valued CL arugment, update to hold value.
          cl_args[cl_name] = cl_value

    print "cl args = %s" % cl_args

    # Report arguments not understood.
    if remaining_args:
      for arg in remaining_args:
        print "Don't understand '%s'" % arg
      sys.exit(1)

    # Read in the decoder tables.
    print "Decoder Generator reading ", table_filename
    f = open(table_filename, 'r')
    decoder = dgen_input.parse_tables(f)

    # Remove tables specified on command line.
    for table in cl_args['table_remove']:
      print 'Removing table %s' % table
      decoder.remove_table(table)

    # Add pattern constraints if specified on the command line.
    if cl_args.get('add-rule-patterns') == 'True':
      decoder = dgen_add_patterns.add_rule_pattern_constraints(decoder)
    f.close()

    print "Successful - got %d tables." % len(decoder.tables())

    print "Generating %s..." % output_filename
    f = open(output_filename, 'w')

    if output_filename.endswith('tests.cc'):
      dgen_test_output.generate_tests_cc(decoder,
                                         decoder_name,
                                         f, cl_args,
                                         cl_args.get('table'))
    elif output_filename.endswith('named_classes.h'):
      dgen_test_output.generate_named_classes_h(
          decoder, decoder_name, _localize_filename(output_filename),
          f, cl_args)
    elif output_filename.endswith('named_decoder.h'):
      dgen_test_output.generate_named_decoder_h(
          decoder, decoder_name, _localize_filename(output_filename),
          f, cl_args)
    elif output_filename.endswith('actuals.h'):
      dgen_actuals.generate_actuals_h(
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
    elif output_filename.endswith('actuals.cc'):
      dgen_actuals.generate_actuals_cc(
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
