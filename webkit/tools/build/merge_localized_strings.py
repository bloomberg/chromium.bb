#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to merge DevTools' localized strings for a list of locales.

Gyp doesn't have any built-in looping capability, so this just provides a way to
loop over a list of locales.
"""

import getopt
import os
import sys

GRIT_DIR = None
L10N_OUT_DIR = None


class Usage(Exception):
  def __init__(self, msg):
    self.msg = msg


def calc_output(locale):
  """Determine the file that will be generated for the given locale."""
  #e.g. '<(PRODUCT_DIR)/resources/inspector/l10n/localizedStrings_da.js',
  return '%s/localizedStrings_%s.js' % (L10N_OUT_DIR, locale)


def calc_inputs(locale):
  """Determine the files that need processing for the given locale."""
  inputs = []

  #e.g. '<(grit_out_dir)/inspectorStrings_da.pak'
  inputs.append('%s/inspectorStrings_%s.js' % (GRIT_DIR, locale))

  #e.g. '<(grit_out_dir)/devtoolsStrings_da.pak'
  inputs.append('%s/devtoolsStrings_%s.js' % (GRIT_DIR, locale))

  return inputs


def list_outputs(locales):
  """Print the names of files that will be generated for the given locales.

  This is to provide gyp the list of output files, so build targets can
  properly track what needs to be built.
  """
  outputs = []
  for locale in locales:
    outputs.append(calc_output(locale))
  # Quote each element so filename spaces don't mess up gyp's attempt to parse
  # it into a list.
  print " ".join(['"%s"' % x for x in outputs])


def list_inputs(locales):
  """Print the names of files that will be processed for the given locales.

  This is to provide gyp the list of input files, so build targets can properly
  track their prerequisites.
  """
  inputs = []
  for locale in locales:
    inputs += calc_inputs(locale)
  # Quote each element so filename spaces don't mess up gyp's attempt to parse
  # it into a list.
  print " ".join(['"%s"' % x for x in inputs])


def merge_localized_strings(locales):
  """ Loop over and repack the given locales."""
  for locale in locales:
    inputs = []
    inputs += calc_inputs(locale)
    output = calc_output(locale)
    print 'Merging %s -> %s' % (inputs, output)
    out = open(output, 'w')
    for inp in inputs:
      out.write('\n// %s\n\n' % (inp))
      out.write(open(inp).read())
    out.close()


def main(argv=None):
  global GRIT_DIR
  global L10N_OUT_DIR

  if argv is None:
    argv = sys.argv

  short_options = 'iog:l:h'
  long_options = 'help'

  print_inputs = False
  print_outputs = False
  usage_msg = ''

  helpstr = """\
Usage:  %s [-h] [-i | -o] -g <DIR> -l <DIR> <locale> [...]
  -h, --help     Print this help, then exit.
  -i             Print the expected input file list, then exit.
  -o             Print the expected output file list, then exit.
  -g DIR         GRIT build files output directory.
  -l DIR         Output dir for l10n files.
  locale [...]   One or more locales.""" % (argv[0])

  try:
    try:
      opts, locales = getopt.getopt(argv[1:], short_options, long_options)
    except getopt.GetoptError, msg:
      raise Usage(str(msg))

    if not locales:
      usage_msg = 'Please specificy at least one locale to process.\n'

    for o, a in opts:
      if o in ('-i'):
        print_inputs = True
      elif o in ('-o'):
        print_outputs = True
      elif o in ('-g'):
        GRIT_DIR = a
      elif o in ('-l'):
        L10N_OUT_DIR = a
      elif o in ('-h', '--help'):
        print helpstr
        return 0

    if not (GRIT_DIR and L10N_OUT_DIR):
      usage_msg += 'Please specify "-g" and "-l".\n'
    if print_inputs and print_outputs:
      usage_msg += 'Please specify only one of "-i" or "-o".\n'

    if usage_msg:
      raise Usage(usage_msg)
  except Usage, err:
    sys.stderr.write(err.msg + '\n')
    sys.stderr.write(helpstr + '\n')
    return 2

  if print_inputs:
    list_inputs(locales)
    return 0

  if print_outputs:
    list_outputs(locales)
    return 0

  merge_localized_strings(locales)

if __name__ == '__main__':
  sys.exit(main(sys.argv))
