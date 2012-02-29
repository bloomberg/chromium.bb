#!/bin/bash

# Filters out "section flags ignored on section redeclaration" warnings. See
# libjpeg.gyp for why.
# http://hans.fugal.net/blog/2008/08/25/filter-stderr/
"$@" 2> >(grep -v "section flags ignored on section redeclaration" 1>&2)
