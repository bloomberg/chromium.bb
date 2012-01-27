#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generator for C++ structs from api json files.

The purpose of this tool is to remove the need for hand-written code that
converts to and from base::Value types when receiving javascript api calls.
Originally written for generating code for extension apis. Reference schemas
are in chrome/common/extensions/api.

Usage example:
  compiler.py --root /home/Work/src --namespace extensions windows.json
    tabs.json
  compiler.py --destdir gen --suffix _api --root /home/Work/src
    --namespace extensions windows.json tabs.json
"""

import cpp_util
import cc_generator
import cpp_type_generator
import h_generator
import json
import model
import optparse
import os.path
import sys

if __name__ == '__main__':
  parser = optparse.OptionParser(
      description='Generates a C++ model of an API from JSON schema',
      usage='usage: %prog [option]... schema [referenced_schema]...')
  parser.add_option('-r', '--root', default='.',
      help='logical include root directory. Path to schema files from specified'
      'dir will be the include path.')
  parser.add_option('-d', '--destdir',
      help='root directory to output generated files.')
  parser.add_option('-n', '--namespace', default='generated_api_schemas',
      help='C++ namespace for generated files. e.g extensions::api.')
  parser.add_option('-s', '--suffix', default='',
      help='Filename and C++ namespace suffix for generated files.')

  (opts, args) = parser.parse_args()
  if not args:
    sys.exit(parser.get_usage())
  dest_dir = opts.destdir
  root_namespace = opts.namespace
  filename_suffix = opts.suffix

  schema = os.path.normpath(args[0])
  referenced_schemas = args[1:]

  api_model = model.Model()

  # Load type dependencies into the model.
  for referenced_schema_path in referenced_schemas:
    with open(referenced_schema_path, 'r') as referenced_schema_file:
      referenced_api_defs = json.loads(referenced_schema_file.read())

    for namespace in referenced_api_defs:
      api_model.AddNamespace(namespace,
          os.path.relpath(referenced_schema_path, opts.root))

  # Actually generate for source file.
  with open(schema, 'r') as schema_file:
    api_defs = json.loads(schema_file.read())

  for target_namespace in api_defs:
    # Gets the relative path from opts.root to the schema to correctly determine
    # the include path.
    relpath = os.path.relpath(schema, opts.root)
    namespace = api_model.AddNamespace(target_namespace, relpath)
    if not namespace:
      continue

    out_file = cpp_util.CppName(namespace.name).lower() + filename_suffix
    type_generator = cpp_type_generator.CppTypeGenerator(root_namespace,
                                                         namespace, out_file)
    for referenced_namespace in api_model.namespaces.values():
      type_generator.AddNamespace(referenced_namespace,
          cpp_util.CppName(referenced_namespace.name).lower() + filename_suffix)
    cc_generator = cc_generator.CCGenerator(namespace, type_generator)
    cc_code = cc_generator.Generate().Render()
    h_generator = h_generator.HGenerator(namespace, type_generator)
    h_code = h_generator.Generate().Render()

    if dest_dir:
      with open(os.path.join(dest_dir, namespace.source_file_dir,
          out_file + '.cc'), 'w') as cc_file:
        cc_file.write(cc_code)
      with open(os.path.join(dest_dir, namespace.source_file_dir,
          out_file + '.h'), 'w') as h_file:
        h_file.write(h_code)
    else:
      print '%s.h' % out_file
      print
      print h_code
      print
      print '%s.cc' % out_file
      print
      print cc_code
