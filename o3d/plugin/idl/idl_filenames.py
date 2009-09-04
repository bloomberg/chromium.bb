#!/usr/bin/env python

# This script takes a list of inputs and generates a list of outputs
# that Nixysa will generate.  It passes through any files not ending
# in .idl, and converts idl files into corresponding .cc and .h files.

# The first argument is the destintation directory for the output
# files, and the rest are relative paths to the source files.  The
# output is posix paths, because that's what GYP expects.

import sys
import posixpath

output_dir = sys.argv[1]

for file in sys.argv[2:]:
  (base, suffix) = posixpath.splitext(file)
  if suffix == ".idl":
    print posixpath.normpath(posixpath.join(output_dir, "%s_glue.h" % base))
    print posixpath.normpath(posixpath.join(output_dir, "%s_glue.cc" % base))
  else:
    print posixpath.normpath(posixpath.join(output_dir, file))

sys.exit(0)
