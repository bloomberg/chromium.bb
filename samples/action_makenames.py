#!/usr/bin/python

# action_makenames.py is a harness script to connect actions sections of
# gyp-based builds to make_names.pl.
#
# usage: action_makenames.py OUTPUTS -- INPUTS [-- OPTIONS]
#
# Multiple OUTPUTS, INPUTS, and OPTIONS may be listed.  The sections are
# separated by -- arguments.
#
# Within OUTPUTS, only the directory name is significant.  It is taken as the
# directory to place output in.  make_names.pl itself is actually responsible
# for choosing the output filename(s), so the basenames of each item listed in
# OUTPUTS is ignored.  This script verifies that if multiple outputs are
# listed, they reside in the same directory.
#
# Multiple INPUTS may be listed.  An input with a basename matching
# "make_names.pl" is taken as the path to that script.  Inputs with names
# ending in TagNames.in or tags.in are taken as tag inputs.  Inputs with names
# ending in AttributeNames.in or attrs.in are taken as attribute inputs.  There
# may be at most one tag input and one attribute input.  A make_names.pl input
# is required and at least one tag or attribute input must be present.
#
# OPTIONS is a list of additional options to pass to make_names.pl.  This
# section need not be present.


import os
import subprocess
import sys


def SplitArgsIntoSections(args):
  sections = []
  while len(args) > 0:
    if not '--' in args:
      # If there is no '--' left, everything remaining is an entire section.
      dashes = len(args)
    else:
      dashes = args.index('--')

    sections.append(args[:dashes])

    # Next time through the loop, look at everything after this '--'.
    if dashes + 1 == len(args):
      # If the '--' is at the end of the list, we won't come back through the
      # loop again.  Add an empty section now corresponding to the nothingness
      # following the final '--'.
      args = []
      sections.append(args)
    else:
      args = args[dashes + 1:]

  return sections


def main(args):
  sections = SplitArgsIntoSections(args[1:])
  assert len(sections) == 2 or len(sections) == 3
  (outputs, inputs) = sections[:2]
  if len(sections) == 3:
    options = sections[2]
  else:
    options = []

  output_dir = os.path.dirname(outputs[0])
  for output in outputs:
    assert os.path.dirname(output) == output_dir

  # Look at the inputs and figure out which ones are make_names.pl, tags, and
  # attributes.  There can be at most one of each, and those are the only
  # input types supported.  make_names.pl is required and at least one of tags
  # and attributes is required.
  make_names_input = None
  tag_input = None
  attr_input = None
  for input in inputs:
    # Make input pathnames absolute so they can be accessed after changing
    # directory.
    input_abs = os.path.abspath(input)
    if os.path.basename(input) == 'make_names.pl':
      assert make_names_input == None
      make_names_input = input_abs
    elif input.endswith('TagNames.in') or input.endswith('tags.in'):
      assert tag_input == None
      tag_input = input_abs
    elif input.endswith('AttributeNames.in') or input.endswith('attrs.in'):
      assert attr_input == None
      attr_input = input_abs
    else:
      assert False

  assert make_names_input != None
  assert tag_input != None or attr_input != None

  # scripts_path is a Perl include directory, located relative to
  # make_names_input.
  scripts_path = os.path.normpath(
      os.path.join(os.path.dirname(make_names_input), os.pardir,
                   'bindings', 'scripts'))

  # Change to the output directory because make_names.pl puts output in its
  # working directory.
  if output_dir != '':
    os.chdir(output_dir)

  # Build up the command.
  command = ['perl', '-I', scripts_path, make_names_input]
  if tag_input != None:
    command.extend(['--tags', tag_input])
  if attr_input != None:
    command.extend(['--attrs', attr_input])
  command.extend(options)

  # Say what's going to be done.
  print subprocess.list2cmdline(command)

  # Do it.  check_call is new in 2.5, so simulate its behavior with call and
  # assert.
  return_code = subprocess.call(command)
  assert return_code == 0

  return return_code


if __name__ == '__main__':
  sys.exit(main(sys.argv))
