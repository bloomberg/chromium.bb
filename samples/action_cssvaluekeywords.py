#!/usr/bin/python

# usage:
# action_cssvaluekeywords.py makevalues.pl \
#    CSSValueKeywords.in SVGCSSValueKeywords.in \
#    CSSValueKeywords.c CSSValueKeywords.gperf CSSValueKeywords.h \
#    CSSValueKeywords.in
# All arguments are pathnames; the first three are inputs and the rest are
# outputs.

import os
import os.path
import subprocess
import sys

assert len(sys.argv) == 8
makevalues_path = sys.argv[1]
in_path = sys.argv[2]
assert os.path.basename(in_path) == 'CSSValueKeywords.in'
svg_in_path = sys.argv[3]
assert os.path.basename(svg_in_path) == 'SVGCSSValueKeywords.in'
out_path = sys.argv[6]
assert os.path.basename(out_path) == 'CSSValueKeywords.h'
merged_path = sys.argv[7]
assert os.path.basename(merged_path) == 'CSSValueKeywords.in'
out_dir = os.path.dirname(out_path)
assert os.path.dirname(merged_path) == out_dir

# Merge in_path and svg_in_path into a single file, CSSValueKeywords.in in
# the working directory that makevalues.pl will run in.
out_file = open(merged_path, 'wb')  # 'wb' to get \n only on windows.

# Make sure there aren't any duplicate lines in CSSValueKeywords.in.  Lowercase
# everything for the check because CSS values are case-insensitive.
line_dict = {}
for path in [in_path, svg_in_path]:
  in_file = open(path)
  for line in in_file:
    line = line.rstrip()
    if line.startswith('#'):
      line = ''
    if line == '':
      continue
    line_lower = line.lower()
    if line_lower in line_dict:
      raise KeyError, 'Duplicate value %s' % line_lower
    line_dict[line_lower] = True
    print >>out_file, line_lower
  in_file.close()

out_file.close()

makevalues_path = os.path.abspath(makevalues_path)

os.chdir(out_dir)

return_code = subprocess.call(['perl', makevalues_path])
assert return_code == 0
