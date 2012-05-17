#!/usr/bin/python
#
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

"""
Responsible for generating the decoder based on parsed
table representations.
"""


import dgen_opt
import dgen_output

# This file generates the class decoder Decoder as defined by the
# decoder tables.  The code is specifically written to minimize the
# number of decoder classes needed to parse valid ARM
# instructions. Many rows in the table use the same decoder class. In
# addition, we optimize tables by merging, so long as the same decoder
# class is built.
#
# The following files are generated:
#
#     decoder.h
#     decoder.cc
#
# decoder.h declares the generated decoder parser class while
# decoder.cc contains the implementation of that decoder class.
#
# For testing purposes (see dgen_test_output.py) different rules are
# applied. Note: It may be worth reading dgen_test_output.py preamble
# to get a better understanding of decoder actions, and why we need
# the "action_filter" methods.

# Defines the header for decoder.h
H_HEADER="""%(FILE_HEADER)s

#ifndef %(IFDEF_NAME)s
#define %(IFDEF_NAME)s

#include "native_client/src/trusted/validator_arm/decode.h"
#include "native_client/src/trusted/validator_arm/actual_classes.h"
#include "native_client/src/trusted/validator_arm/baseline_classes.h"

namespace nacl_arm_dec {
"""

DECODER_DECLARE_HEADER="""
// Defines a decoder class selector for instructions.
class %(decoder_name)s : DecoderState {
 public:
   explicit %(decoder_name)s();
   virtual ~%(decoder_name)s();

   // Parses the given instruction, returning the decoder to use.
   virtual const ClassDecoder& decode(const Instruction) const;

 private:
"""

DECODER_DECLARE_METHOD_COMMENTS="""
  // The following list of methods correspond to each decoder table,
  // and implements the pattern matching of the corresponding bit
  // patterns. After matching the corresponding bit patterns, they
  // either call other methods in this list (corresponding to another
  // decoder table), or they return the instance field that implements
  // the class decoder that should be used to decode the particular
  // instruction.
"""

DECODER_DECLARE_METHOD="""
  inline const ClassDecoder& decode_%(table_name)s(
      const Instruction insn) const;
"""

DECODER_DECLARE_FIELD_COMMENTS="""
  // The following fields define the set of class decoders
  // that can be returned by the API function "decode". They
  // are created once as instance fields, and then returned
  // by the table methods above. This speeds up the code since
  // the class decoders need to only be built once (and reused
  // for each call to "decode")."""

DECODER_DECLARE_FIELD="""
  const %(decoder)s %(decoder)s_instance_;"""

DECODER_DECLARE_FOOTER="""
};
"""

H_FOOTER="""
}  // namespace nacl_arm_dec
#endif  // %(IFDEF_NAME)s
"""

def generate_h(decoder, decoder_name, filename, out):
    """Entry point to the decoder for .h file.

    Args:
        decoder: The decoder defined by the list of Table objects to process.
        decoder_name: The name of the decoder state to build.
        filename: The (localized) name for the .h file.
        named_decoders: If true, generate a decoder state with named instances.
        out: a COutput object to write to.
    """
    if not decoder.primary: raise Exception('No tables provided.')

    # Before starting, remove all testing information from the parsed tables.
    decoder = decoder.action_filter(['actual'])

    values = {
        'FILE_HEADER': dgen_output.HEADER_BOILERPLATE,
        'IFDEF_NAME': dgen_output.ifdef_name(filename),
        'decoder_name': decoder_name,
        }
    out.write(H_HEADER % values)
    out.write(DECODER_DECLARE_HEADER % values)
    out.write(DECODER_DECLARE_METHOD_COMMENTS)
    for table in decoder.tables():
      values['table_name'] = table.name
      out.write(DECODER_DECLARE_METHOD % values)
    out.write(DECODER_DECLARE_FIELD_COMMENTS)
    for action in decoder.decoders():
      values['decoder'] = action.actual;
      out.write(DECODER_DECLARE_FIELD % values)
    out.write(DECODER_DECLARE_FOOTER % values)
    out.write(H_FOOTER % values)

# Defines the header for DECODER.h
CC_HEADER="""%(FILE_HEADER)s

#include "%(header_filename)s"

#include <stdio.h>

namespace nacl_arm_dec {

"""

CONSTRUCTOR_HEADER="""
%(decoder_name)s::%(decoder_name)s() : DecoderState()"""

CONSTRUCTOR_FIELD_INIT="""
  , %(decoder)s_instance_()"""

CONSTRUCTOR_FOOTER="""
{}

%(decoder_name)s::~%(decoder_name)s() {}
"""

METHOD_HEADER="""
// Implementation of table: %(table_name)s.
// Specified by: %(citation)s
const ClassDecoder& %(decoder_name)s::decode_%(table_name)s(
     const Instruction insn) const
{
"""

METHOD_DISPATCH_BEGIN="""
  if (%s"""

METHOD_DISPATCH_CONTINUE=""" &&
      %s"""

METHOD_DISPATCH_END=")"""

METHOD_DISPATCH_CLASS_DECODER="""
    return %(decoder)s_instance_;
"""

METHOD_DISPATCH_SUBMETHOD="""
    return decode_%(subtable_name)s(insn);
"""

METHOD_FOOTER="""
  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: %(table_name)s could not parse %%08X",
          insn.Bits());
  return Forbidden_instance_;
}
"""

DECODER_METHOD="""
const ClassDecoder& %(decoder_name)s::decode(const Instruction insn) const {
  return decode_%(entry_table_name)s(insn);
}
"""

CC_FOOTER="""
}  // namespace nacl_arm_dec
"""

def generate_cc(decoder, decoder_name, filename, out):
    """Implementation of the decoder in .cc file

    Args:
        decoder: The decoder defined by the list of Table objects to process.
        decoder_name: The name of the decoder state to build.
        filename: The (localized) name for the .h file.
        named_decoders: If true, generate a decoder state with named instances.
        out: a COutput object to write to.
    """
    if not decoder.primary: raise Exception('No tables provided.')
    assert filename.endswith('.cc')

    # Before starting, remove all testing information from the parsed tables.
    decoder = decoder.action_filter(['actual'])
    values = {
        'FILE_HEADER': dgen_output.HEADER_BOILERPLATE,
        'header_filename': filename[:-2] + 'h',
        'decoder_name': decoder_name,
        'entry_table_name': decoder.primary.name,
        }
    out.write(CC_HEADER % values)
    _generate_constructors(decoder, values, out)
    _generate_methods(decoder, values, out)
    out.write(DECODER_METHOD % values)
    out.write(CC_FOOTER % values)

def _generate_constructors(decoder, values, out):
  out.write(CONSTRUCTOR_HEADER % values)
  for decoder in decoder.decoders():
    values['decoder'] = decoder.actual
    out.write(CONSTRUCTOR_FIELD_INIT % values)
  out.write(CONSTRUCTOR_FOOTER % values)

def _generate_methods(decoder, values, out):
  for table in decoder.tables():
    # Add the default row as the last in the optimized row, so that
    # it is applied if all other rows do not.
    opt_rows = sorted(dgen_opt.optimize_rows(table.rows(False)))
    if table.default_row:
      opt_rows.append(table.default_row)

    opt_rows = table.add_column_to_rows(opt_rows)
    print ("Table %s: %d rows minimized to %d"
           % (table.name, len(table.rows()), len(opt_rows)))

    values['table_name'] = table.name
    values['citation'] = table.citation
    out.write(METHOD_HEADER % values)


    # Add message to stop compilation warnings if this table
    # doesn't require subtables to select a class decoder.
    if not table.methods():
      out.write("  UNREFERENCED_PARAMETER(insn);")

    for row in opt_rows:
      # Each row consists of a set of bit patterns defining if the row
      # is applicable. Convert this into a sequence of anded C test
      # expressions. For example, convert the following pair of bit
      # patterns:
      #
      #   xxxx1010xxxxxxxxxxxxxxxxxxxxxxxx
      #   xxxxxxxxxxxxxxxxxxxxxxxxxxxx0101
      #
      # Each instruction is masked to get the the bits, and then
      # tested against the corresponding expected bits. Hence, the
      # above example is converted to:
      #
      #    ((insn & 0x0F000000) != 0x0C000000) &&
      #    ((insn & 0x0000000F) != 0x00000005)
      out.write(METHOD_DISPATCH_BEGIN %
                row.patterns[0].to_c_expr('insn.Bits()'))
      for p in row.patterns[1:]:
        out.write(METHOD_DISPATCH_CONTINUE % p.to_c_expr('insn.Bits()'))
      out.write(METHOD_DISPATCH_END)
      if row.action.__class__.__name__ == 'DecoderAction':
        values['decoder'] = row.action.actual
        out.write(METHOD_DISPATCH_CLASS_DECODER % values)
      elif row.action.__class__.__name__ == 'DecoderMethod':
        values['subtable_name'] = row.action.name
        out.write(METHOD_DISPATCH_SUBMETHOD % values)
      else:
        raise Exception('Bad table action: %s' % repr(row.action))
    out.write(METHOD_FOOTER % values)
