#!/usr/bin/python

# usage: rule_binding.py INPUT OUTDIR -- INPUTS -- OPTIONS
#
# INPUT is an IDL file, such as Whatever.idl.
#
# OUTDIR is the directory into which V8Whatever.cpp and V8Whatever.h will be
# placed.
#
# The first item in INPUTS is the path to generate-bindings.pl.  Remaining
# items in INPUTS are used to build the Perl module include path.
#
# OPTIONS are passed as-is to generate-bindings.pl as additional arguments.


import errno
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
  assert len(sections) == 3
  (base, inputs, options) = sections

  assert len(base) == 2
  input = base[0]
  outdir = base[1]

  assert len(inputs) > 1
  generate_bindings = inputs[0]
  perl_modules = inputs[1:]

  include_dirs = []
  for perl_module in perl_modules:
    include_dir = os.path.dirname(perl_module)
    if not include_dir in include_dirs:
      include_dirs.append(include_dir)

  # Make sure the output directory exists.
  try:
    os.makedirs(outdir)
  except OSError, e:
    if e.errno != errno.EEXIST:
      raise

  # Build up the command.
  command = ['perl', '-w']
  for include_dir in include_dirs:
    command.extend(['-I', include_dir])
  command.append(generate_bindings)
  command.extend(options)
  command.extend(['--outputdir', outdir, input])

  # Say what's going to be done.
  print subprocess.list2cmdline(command)

  # Do it.  check_call is new in 2.5, so simulate its behavior with call and
  # assert.
  return_code = subprocess.call(command)
  assert return_code == 0

  return return_code


if __name__ == '__main__':
  sys.exit(main(sys.argv))
