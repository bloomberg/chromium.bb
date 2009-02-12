#!/usr/bin/python

# usage:
# action_csspropertynames.py makeprop.pl \
#    CSSPropertyNames.in SVGCSSPropertyNames.in \
#    CSSPropertyNames.cpp CSSPropertyNames.gperf CSSPropertyNames.h \
#    CSSPropertyNames.in
# All arguments are pathnames; the first three are inputs and the rest are
# outputs.

import os
import os.path
import subprocess
import sys

assert len(sys.argv) == 8
makeprop_path = sys.argv[1]
in_path = sys.argv[2]
assert os.path.basename(in_path) == 'CSSPropertyNames.in'
svg_in_path = sys.argv[3]
assert os.path.basename(svg_in_path) == 'SVGCSSPropertyNames.in'
out_path = sys.argv[6]
assert os.path.basename(out_path) == 'CSSPropertyNames.h'
merged_path = sys.argv[7]
assert os.path.basename(merged_path) == 'CSSPropertyNames.in'
out_dir = os.path.dirname(out_path)
assert os.path.dirname(merged_path) == out_dir

# Merge in_path and svg_in_path into a single file, CSSPropertyNames.in in
# the working directory that makeprop.pl will run in.
out_file = open(merged_path, 'w')

# Make sure there aren't any duplicate lines in CSSPropertyNames.in.
line_dict = {}
for path in [in_path, svg_in_path]:
  in_file = open(path)
  for line in in_file:
    line = line.rstrip()
    if line.startswith('#'):
      line = ''
    if line == '':
      continue
    if line in line_dict:
      raise KeyError, 'Duplicate value %s' % line
    line_dict[line] = True
    print >>out_file, line
  in_file.close()

out_file.close()

makeprop_path = os.path.abspath(makeprop_path)

os.chdir(out_dir)

return_code = subprocess.call(['perl', makeprop_path])
assert return_code == 0
