#!/usr/bin/python
#
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

"""
Responsible for generating actual decoders based on parsed table
representations.

Note: We define a notion of "actual" and "baseline" classes to
distinquish how they are used.

"baseline" instruction decoders are defined for each instruction that
appears in the manual. The "baseline" decoder captures the properties
of the instruction through the corresponding set of defined virtuals
(as defined in "inst_classes.h"). "baseline" instruction decoders are
used to test that individual instructions are decoded as expected.

"actual" instruction decoders are merged "baseline" instruction
decoders, and are used in the production instruction decoder.  The
main purpose of merging instruction decoders into corresponding actual
instruction decoders is to minimize production code size, by
minimizing the number of implementations that must be used by the
production intruction decoder.

To verify that actual and baseline instruction decoders are
equivalent, we test all possible instruction patterns and verify that
the baseline and actual instruction decoders behave the same way
(modulo a concept on whether we cared if condition codes were
assigned).

The main problem with the initial implementation of baseline and
actual instruction decoders is that all are hand written, and the
relationship between the baseline and corresponding actual decoders is
also determined by hand.

To simplify the maintenance of actual instruction decoders, this file
merges baseline classes, based on having the same set of defined
virtual functions (as defined by the corresponding fields in the
decoder tables within file "armv7.table"). The advantage of using this
notion is that changes only need be defined in "armv7.table", and the
generator will figure out how baseline classes are merged (rather than
in the old system where everything was updated by hand).

Verification of the new actual decoders will be the same as before.
We test all possible instruction patterns and verify that the baseline
and actual instruction decoders behave the same way.
"""

import dgen_core
import dgen_decoder
import dgen_output

# Holds the decoder that actuals are defined on.
ACTUAL_DECODER = None

# Holds the map from baseline decoder to the corresponding
# actual decoder.
BASELINE_TO_ACTUAL_MAP = {}

# Holds the map from an actual decoder to the corresponding
# (sorted) set of baseline decoders.
ACTUAL_TO_BASELINE_MAP = {}

# Holds the map from baseline decoder name, to the corresponding
# (sorted) list of baseline decoders with that name.
BASELINE_NAME_TO_BASELINE_MAP = {}

# Holds the map from an actual, to the name we will use for it.
ACTUAL_TO_NAME_MAP = {}

def GetActualDecoders(decoder):
  """Takes the given decoder table, and builds the corresponding
     internal maps, so that we can consistently name actual classes.
     Returns the (sorted) list of actual decoders to build.
     """
  global ACTUAL_DECODER

  # Verify whether actual classes have already been recorded.
  if ACTUAL_DECODER:
    raise Exception("GetActualDecoders: Multiple decoders not allowed.")

  ACTUAL_DECODER = decoder
  actuals = set()

  # Get the list of decoder (actions) defined in the decoder table.
  for baseline in decoder.decoders():
    if not dgen_decoder.ActionDefinesDecoder(baseline): continue

    actual = dgen_decoder.BaselineToActual(baseline)
    actuals.add(actual)

    _AddBaselineToBaselineNameToBaselineMap(baseline)
    _AddToBaselineToActualMap(baseline, actual)
    _AddToActualToBaselineMap(baseline, actual)

  _FixActualToBaselineMap()
  _FixBaselineNameToBaselineMap()
  _DefineActualNames(actuals)
  return sorted(actuals, key=ActualName)

def _DefineActualNames(actuals):
  """Installs a unique name for each actual, based on the baseline decoders
     associated with it.
     """
  global ACTUAL_TO_NAME_MAP

  name_map = {}
  for actual in sorted(actuals):
    bases = ACTUAL_TO_BASELINE_MAP[actual]
    name = 'Unnamed'
    if bases:
      baseline = bases[0]
      name = dgen_decoder.BaselineName(baseline)
    count = name_map.get(name)
    if count == None:
      count = 1
    actual_name = 'Actual_%s_case_%s' % (name, count)
    name_map[name] = count + 1
    ACTUAL_TO_NAME_MAP[actual] = actual_name

def ActualName(actual):
  """Returns the name to use for the actual."""
  return ACTUAL_TO_NAME_MAP[actual]

ACTUAL_H_HEADER="""%(FILE_HEADER)s
#ifndef %(IFDEF_NAME)s
#define %(IFDEF_NAME)s

#include "native_client/src/trusted/validator_arm/inst_classes.h"

namespace nacl_arm_dec {
"""

ACTUAL_H_FOOTER="""
} // namespace nacl_arm_test

#endif  // %(IFDEF_NAME)s
"""

def generate_actuals_h(decoder, decoder_name, filename, out, cl_args):
  """Generates actual decoder C++ declarations in the given file.

    Args:
        decoder: The decoder tables.
        decoder_name: The name of the decoder state to build.
        filename: The (localized) name for the .h file.
        out: a COutput object to write to.
        cl_args: A dictionary of additional command line arguments.
        """
  if not decoder.primary: raise Exception('No tables provided.')

  assert filename.endswith('actuals.h')

  values = {
      'FILE_HEADER': dgen_output.HEADER_BOILERPLATE,
      'IFDEF_NAME' : dgen_output.ifdef_name(filename),
      'decoder_name': decoder_name,
      }
  out.write(ACTUAL_H_HEADER % values)
  _print_actual_headers(GetActualDecoders(decoder), out)
  out.write(ACTUAL_H_FOOTER % values)

ACTUAL_CC_HEADER="""%(FILE_HEADER)s
#include "native_client/src/trusted/validator_arm/inst_classes.h"
#include "native_client/src/trusted/validator_arm/gen/arm32_decode_actuals.h"

namespace nacl_arm_dec {
"""

ACTUAL_CC_FOOTER="""
}  // namespace nacl_arm_dec
"""

def generate_actuals_cc(decoder, decoder_name, filename, out, cl_args):
  """Generates the actual decoder C++ definitions in the given file.

    Args:
        decoder: The decoder tables.
        decoder_name: The name of the decoder state to build.
        filename: The (localized) name for the .h file.
        out: a COutput object to write to.
        cl_args: A dictionary of additional command line arguments.
        """

  if not decoder.primary: raise Exception('No tables provided.')

  assert filename.endswith('actuals.cc')

  values = {
      'FILE_HEADER': dgen_output.HEADER_BOILERPLATE,
      'decoder_name': decoder_name,
      }

  out.write(ACTUAL_CC_HEADER % values)
  _print_actual_classes(GetActualDecoders(decoder), out)
  out.write(ACTUAL_CC_FOOTER % values)

ACTUAL_CLASS_HEADER="""
// %(decoder_name)s
//
// Actual:
//  %(actual_rep)s"""

ACTUAL_CLASS_REP="""
//
// Baseline:
//  %(baseline_rep)s"""

def _print_actual_headers(actuals, out):
  """Generates C++ class declarations for each of the given actual decoders."""
  for actual in actuals:
    actual_name = ActualName(actual)
    values = {
        'decoder_name': actual_name,
        'actual_rep':  dgen_decoder.commented_decoder_neutral_repr(actual)
        }
    out.write(ACTUAL_CLASS_HEADER % values)
    for baseline in ACTUAL_TO_BASELINE_MAP[actual]:
      values['baseline_rep'] = (
          dgen_decoder.commented_decoder_repr(baseline))
      out.write(ACTUAL_CLASS_REP % values)
    dgen_decoder.DeclareDecoder(actual, actual_name, out)


ACTUAL_CLASS_DEF_HEADER="""
// %(decoder_name)s
//
// Actual:
//  %(actual_rep)s
"""

def _print_actual_classes(actuals, out):
  """Generates C++ class definitions for each of the given actual decoders."""
  for actual in actuals:
    actual_name = ActualName(actual)
    values = {
        'decoder_name': actual_name,
        'actual_rep': dgen_decoder.commented_decoder_neutral_repr(actual),
        }
    out.write(ACTUAL_CLASS_DEF_HEADER % values)
    dgen_decoder.DefineDecoder(actual, actual_name, out)

def _AddBaselineToBaselineNameToBaselineMap(baseline):
  """Add entry into BASELINE_NAME_TO_BASELINE_NAME_MAP for the
     given baseline class."""
  baseline_name = dgen_decoder.BaselineName(baseline)
  bases = BASELINE_NAME_TO_BASELINE_MAP.get(baseline_name)
  if bases == None:
    bases = set()
    BASELINE_NAME_TO_BASELINE_MAP[baseline_name] = bases
  bases.add(baseline)

def _FixBaselineNameToBaselineMap():
  """Replaces the sets in BASELINE_NAME_TO_BASELINE_MAP with
     corresponding sorted lists."""
  for baseline_name in BASELINE_NAME_TO_BASELINE_MAP.keys():
    BASELINE_NAME_TO_BASELINE_MAP[baseline_name] = sorted(
        BASELINE_NAME_TO_BASELINE_MAP[baseline_name])

def _AddToBaselineToActualMap(baseline, actual):
  """Add given entry to BASELINE_TO_ACTUAL_MAP."""
  BASELINE_TO_ACTUAL_MAP[baseline] = actual

def _AddToActualToBaselineMap(baseline, actual):
  """Add given entry to ACTUAL_TO_BASELINE_MAP."""
  bases = ACTUAL_TO_BASELINE_MAP.get(actual)
  if bases == None:
    bases = set()
    ACTUAL_TO_BASELINE_MAP[actual] = bases
  bases.add(baseline)

def _FixActualToBaselineMap():
  """Replace the sets in ACTUAL_TO_BASELINE_MAP with corresponding
     sorted lists."""
  for actual in ACTUAL_TO_BASELINE_MAP.keys():
    ACTUAL_TO_BASELINE_MAP[actual] = sorted(
        ACTUAL_TO_BASELINE_MAP[actual],
        key=dgen_decoder.BaselineNameAndBaseline)
