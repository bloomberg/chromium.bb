# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Convert PrivateScript's sources to C++ constant strings.
FIXME: We don't want to add more build scripts. Rewrite this script in grit. crbug.com/388121

Usage:
python make_private_script_source.py DESTINATION_FILE SOURCE_FILES
"""

import os
import sys


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
            contents.append('const unsigned char kSourceOf%s[] = {\n    %s\n};\n' % (
                class_name, ', '.join(hex_values)))
    contents.append('struct %s {' % source_name)
    contents.append("""
    const char* name;
    const unsigned char* source;
    size_t size;
};

""")
    contents.append('struct %s k%s[] = {\n' % (source_name, source_name))
    for input_filename in input_filenames:
        class_name, ext = os.path.splitext(os.path.basename(input_filename))
        contents.append('    { "%s", kSourceOf%s, sizeof(kSourceOf%s) },\n' % (class_name, class_name, class_name))
    contents.append('};\n')

    with open(output_filename, 'w') as output_file:
        output_file.write("".join(contents))


if __name__ == '__main__':
    sys.exit(main())
