#!/usr/bin/python

# usage:
# action_csspropertynames.py makeprop.pl CSSPropertyNames.in CSSPropertyNames.h
# All arguments are pathnames; the first two are inputs and the last is an
# output.

import os
import os.path
import shutil
import subprocess
import sys

print sys.argv
assert len(sys.argv) == 6
makeprop_path = sys.argv[1]
in_path = sys.argv[2]
assert os.path.basename(in_path) == 'CSSPropertyNames.in'
out_path = sys.argv[5]
assert os.path.basename(out_path) == 'CSSPropertyNames.h'
out_dir = os.path.dirname(out_path)

# Make sure there aren't any duplicate lines in CSSPropertyNames.in.
line_dict = {}
in_file = open(in_path)
line_number = 0
for line in in_file:
  line_number = line_number + 1
  line = line.rstrip()
  if line.startswith('#'):
    line = ''
  if line == '':
    continue
  if line in line_dict:
    raise KeyError, 'Duplicate value in %s at lines %d and %d' % \
                    (in_path, line_dict[line], line_number)
  line_dict[line] = line_number
in_file.close()

# makeprop.pl works in the current directory, so copy the input to the
# directory where the output needs to be produced, and then change there.
# Make the makeprop path absolute first so that it can still be referenced.
shutil.copyfile(in_path, os.path.join(out_dir, 'CSSPropertyNames.in'))

makeprop_path = os.path.abspath(makeprop_path)

os.chdir(out_dir)

return_code = subprocess.call(['perl', makeprop_path])
assert return_code == 0

# Remove the copy of the input file.
os.unlink('CSSPropertyNames.in')
