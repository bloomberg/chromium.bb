#!/usr/bin/python

# usage:
# action_htmlnames.py make_names.pl HTMLTagNames.in HTMLAttributeNames.in \
#                     HTMLNames.cpp HTMLNames.h EXTRA_DEFINES
# The first three arguments are input pathnames.  The fourth and fifth are
# output pathnames.  The sixth is something else passed to make_names.pl.

import os
import os.path
import shutil
import subprocess
import sys

assert len(sys.argv) == 7
make_names_path = sys.argv[1]
tag_names_path = sys.argv[2]
assert os.path.basename(tag_names_path) == 'HTMLTagNames.in'
attribute_names_path = sys.argv[3]
assert os.path.basename(attribute_names_path) == 'HTMLAttributeNames.in'
html_names_path = sys.argv[4]
assert os.path.basename(html_names_path) == 'HTMLNames.cpp'
out_dir = os.path.dirname(html_names_path)
html_names_h_path = sys.argv[5]
assert os.path.basename(html_names_h_path) == 'HTMLNames.h'
assert out_dir == os.path.dirname(html_names_h_path)
extra_defines = sys.argv[6]

# make_names.pl works in the current directory, make the paths absolute.
make_names_path = os.path.abspath(make_names_path)
tag_names_path = os.path.abspath(tag_names_path)
attribute_names_path = os.path.abspath(attribute_names_path)

scripts_path = os.path.join(os.path.dirname(os.path.dirname(make_names_path)),
                            'bindings', 'scripts')

os.chdir(out_dir)

return_code = subprocess.call(['perl', '-I', scripts_path, make_names_path,
                               '--tags', tag_names_path,
                               '--attrs', attribute_names_path,
                               '--wrapperFactory',
                               '--extraDefines', extra_defines])
assert return_code == 0
