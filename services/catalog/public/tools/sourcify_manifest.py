#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates a C++ source file which defines a string constant containing the
contents of a catalog manifest file. Useful for baking catalogs into binaries
which don't want to hit disk before initializing the catalog."""

import argparse
import json
import os.path
import sys


# Token used to delimit raw strings in the generated source file. It's illegal
# for this token to appear within the contents of the input manifest itself.
_RAW_STRING_DELIMITER = "#CATALOG_JSON#"


def main():
  parser = argparse.ArgumentParser(
      description="Generates a C++ constant containing a catalog manifest.")
  parser.add_argument("--input")
  parser.add_argument("--output")
  parser.add_argument("--symbol-name")
  parser.add_argument("--pretty", action="store_true")
  args, _ = parser.parse_known_args()

  if args.input is None or args.output is None or args.symbol_name is None:
    raise Exception("--input, --output, and --symbol-name are required")

  with open(args.input, 'r') as input_file:
    manifest_contents = input_file.read()

  if manifest_contents.find(_RAW_STRING_DELIMITER) >= 0:
    raise Exception(
        "Unexpected '%s' found in input manifest." % _RAW_STRING_DELIMITER)

  qualified_symbol_name = args.symbol_name.split("::")
  namespace = qualified_symbol_name[0:-1]
  symbol_name = qualified_symbol_name[-1]

  with open(args.output, 'w') as output_file:
    output_file.write(
        "// This is a generated file produced by\n"
        "// src/services/catalog/public/tools/sourcify_manifest.py.\n\n")
    for name in namespace:
      output_file.write("namespace %s {\n" % name)
    output_file.write("\nextern const char %s[];" % symbol_name)
    output_file.write("\nconst char %s[] = R\"%s(%s)%s\";\n\n" %
        (symbol_name, _RAW_STRING_DELIMITER, manifest_contents,
            _RAW_STRING_DELIMITER))
    for name in reversed(namespace):
      output_file.write("}  // %s\n" % name)

  return 0

if __name__ == "__main__":
  sys.exit(main())
