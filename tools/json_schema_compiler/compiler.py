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
  compiler.py --destdir gen --root /home/Work/src
    --namespace extensions windows.json tabs.json
"""

import cc_generator
import cpp_type_generator
import h_generator
import idl_schema
import json_schema
import model
import schema_bundle_generator

import optparse
import os.path
import sys

def load_schema(schema):
  schema_filename, schema_extension = os.path.splitext(schema)

  if schema_extension == '.json':
    api_defs = json_schema.Load(schema)
  elif schema_extension == '.idl':
    api_defs = idl_schema.Load(schema)
  else:
    sys.exit("Did not recognize file extension %s for schema %s" %
             (schema_extension, schema))
  if len(api_defs) != 1:
    sys.exit("File %s has multiple schemas. Files are only allowed to contain a"
             " single schema." % schema)

  return api_defs

def handle_single_schema(filename, dest_dir, root, root_namespace):
  schema = os.path.normpath(filename)
  schema_filename, schema_extension = os.path.splitext(schema)
  path, short_filename = os.path.split(schema_filename)
  api_defs = json_schema.DeleteNocompileNodes(load_schema(schema))

  api_model = model.Model()

  for target_namespace in api_defs:
    referenced_schemas = target_namespace.get('dependencies', [])
    # Load type dependencies into the model.
    # TODO(miket): do we need this in IDL?
    for referenced_schema in referenced_schemas:
      split_schema = referenced_schema.split(':', 1)
      if len(split_schema) > 1:
        if split_schema[0] != 'api':
          continue
        else:
          referenced_schema = split_schema[1]

      referenced_schema_path = os.path.join(
          os.path.dirname(schema), referenced_schema + '.json')
      referenced_api_defs = json_schema.Load(referenced_schema_path)

      for namespace in referenced_api_defs:
        api_model.AddNamespace(namespace,
            os.path.relpath(referenced_schema_path, opts.root))

    # Gets the relative path from opts.root to the schema to correctly determine
    # the include path.
    relpath = os.path.relpath(schema, opts.root)
    namespace = api_model.AddNamespace(target_namespace, relpath)
    if not namespace:
      continue

    if short_filename != namespace.unix_name:
      sys.exit("Filename %s is illegal. Name files using unix_hacker style." %
               filename)

    # The output filename must match the input filename for gyp to deal with it
    # properly.
    out_file = namespace.unix_name
    type_generator = cpp_type_generator.CppTypeGenerator(
        root_namespace, namespace, namespace.unix_name)
    for referenced_namespace in api_model.namespaces.values():
      if referenced_namespace == namespace:
        continue
      type_generator.AddNamespace(
          referenced_namespace,
          referenced_namespace.unix_name)

    h_code = (h_generator.HGenerator(namespace, type_generator)
        .Generate().Render())
    cc_code = (cc_generator.CCGenerator(namespace, type_generator)
        .Generate().Render())

    if dest_dir:
      with open(
          os.path.join(dest_dir, namespace.source_file_dir, out_file + '.cc'),
          'w') as cc_file:
        cc_file.write(cc_code)
      with open(
          os.path.join(dest_dir, namespace.source_file_dir, out_file + '.h'),
          'w') as h_file:
        h_file.write(h_code)
    else:
      print '%s.h' % out_file
      print
      print h_code
      print
      print '%s.cc' % out_file
      print
      print cc_code

def handle_bundle_schema(filenames, dest_dir, root, root_namespace):
  # Merge the source files into a single list of schemas.
  api_defs = []
  for filename in filenames:
    schema = os.path.normpath(filename)
    schema_filename, schema_extension = os.path.splitext(schema)
    api_defs.extend(load_schema(schema))

  api_model = model.Model()
  relpath = os.path.relpath(os.path.normpath(filenames[0]), root)

  for target_namespace, schema_filename in zip(api_defs, filenames):
    namespace = api_model.AddNamespace(target_namespace, relpath)
    path, filename = os.path.split(schema_filename)
    short_filename, extension = os.path.splitext(filename)

    # Filenames are checked against the unix_names of the namespaces they
    # generate because the gyp uses the names of the JSON files to generate
    # the names of the .cc and .h files. We want these to be using unix_names.
    if namespace.unix_name != short_filename:
      sys.exit("Filename %s is illegal. Name files using unix_hacker style." %
               schema_filename)

  type_generator = cpp_type_generator.CppTypeGenerator(root_namespace)
  for referenced_namespace in api_model.namespaces.values():
    type_generator.AddNamespace(
        referenced_namespace,
        referenced_namespace.unix_name)

  generator = schema_bundle_generator.SchemaBundleGenerator(
      api_model, api_defs, type_generator)
  api_h_code = generator.GenerateAPIHeader().Render()
  schemas_h_code = generator.GenerateSchemasHeader().Render()
  schemas_cc_code = generator.GenerateSchemasCC().Render()

  if dest_dir:
    basedir = os.path.join(dest_dir, 'chrome/common/extensions/api')
    with open(os.path.join(basedir, 'generated_api.h'), 'w') as h_file:
      h_file.write(api_h_code)
    with open(os.path.join(basedir, 'generated_schemas.h'), 'w') as h_file:
      h_file.write(schemas_h_code)
    with open(os.path.join(basedir, 'generated_schemas.cc'), 'w') as cc_file:
      cc_file.write(schemas_cc_code)
  else:
    print 'generated_api.h'
    print
    print api_h_code
    print
    print 'generated_schemas.h'
    print
    print schemas_h_code
    print
    print 'generated_schemas.cc'
    print
    print schemas_cc_code

if __name__ == '__main__':
  parser = optparse.OptionParser(
      description='Generates a C++ model of an API from JSON schema',
      usage='usage: %prog [option]... schema')
  parser.add_option('-r', '--root', default='.',
      help='logical include root directory. Path to schema files from specified'
      'dir will be the include path.')
  parser.add_option('-d', '--destdir',
      help='root directory to output generated files.')
  parser.add_option('-n', '--namespace', default='generated_api_schemas',
      help='C++ namespace for generated files. e.g extensions::api.')
  parser.add_option('-b', '--bundle', action="store_true", help=
'''if supplied, causes compiler to generate bundle files for the given set of
source files.''')

  (opts, args) = parser.parse_args()

  if not args:
    sys.exit(0) # This is OK as a no-op
  dest_dir = opts.destdir
  root_namespace = opts.namespace

  if opts.bundle:
    handle_bundle_schema(args, dest_dir, opts.root, root_namespace)
  else:
    handle_single_schema(args[0], dest_dir, opts.root, root_namespace)
