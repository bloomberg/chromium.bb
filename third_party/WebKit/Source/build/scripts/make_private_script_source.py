# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Convert PrivateScript's sources to C++ constant strings.
FIXME: We don't want to add more build scripts. Rewrite this script in grit. crbug.com/388121

Usage:
python make_private_script_source.py DESTINATION_FILE SOURCE_FILES
"""

import os
import re
import sys


# We assume that X.js has a corresponding X.idl in the same directory.
# If X is a partial interface, this method extracts the base name of the partial interface from X.idl.
# Otherwise, this method returns None.
def extract_partial_interface_name(filename):
    basename, ext = os.path.splitext(filename)
    assert ext == '.js'
    # PrivateScriptRunner.js is a special JS script to control private scripts,
    # and doesn't have a corresponding IDL file.
    if os.path.basename(basename) == 'PrivateScriptRunner':
        return None
    idl_filename = basename + '.idl'
    with open(idl_filename) as f:
        contents = f.read()
        match = re.search(r'partial\s+interface\s+(\w+)\s*{', contents)
        return match and match.group(1)


def main():
    output_filename = sys.argv[1]
    input_filenames = sys.argv[2:]
    source_name, ext = os.path.splitext(os.path.basename(output_filename))

    contents = []
    for input_filename in input_filenames:
        class_name, ext = os.path.splitext(os.path.basename(input_filename))
        with open(input_filename) as input_file:
            input_text = input_file.read()
            hex_values = ['0x{0:02x}'.format(ord(char)) for char in input_text]
            contents.append('const unsigned char kSourceOf%s[] = {\n    %s\n};\n\n' % (
                class_name, ', '.join(hex_values)))
    contents.append('struct %s {' % source_name)
    contents.append("""
    const char* scriptClassName;
    const char* className;
    const unsigned char* source;
    size_t size;
};

""")
    contents.append('struct %s k%s[] = {\n' % (source_name, source_name))
    for input_filename in input_filenames:
        script_class_name, ext = os.path.splitext(os.path.basename(input_filename))
        class_name = extract_partial_interface_name(input_filename) or script_class_name
        contents.append('    { "%s", "%s", kSourceOf%s, sizeof(kSourceOf%s) },\n' % (script_class_name, class_name, script_class_name, script_class_name))
    contents.append('};\n')

    with open(output_filename, 'w') as output_file:
        output_file.write("".join(contents))


if __name__ == '__main__':
    sys.exit(main())
