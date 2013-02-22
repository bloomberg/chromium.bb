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

from cpp_generator import CppGenerator
from cpp_type_generator import CppTypeGenerator
from dart_generator import DartGenerator
from cpp_bundle_generator import CppBundleGenerator
from model import Model
import idl_schema
import json_schema

import optparse
import os.path
import sys

# Names of supported code generators, as specified on the command-line.
# First is default.
GENERATORS = ['cpp', 'cpp-bundle', 'dart']

def _LoadSchema(schema):
  schema_filename, schema_extension = os.path.splitext(schema)

  if schema_extension == '.json':
    api_defs = json_schema.Load(schema)
  elif schema_extension == '.idl':
    api_defs = idl_schema.Load(schema)
  else:
    sys.exit('Did not recognize file extension %s for schema %s' %
             (schema_extension, schema))
  if len(api_defs) != 1:
    sys.exit('File %s has multiple schemas. Files are only allowed to contain a'
             ' single schema.' % schema)

  return api_defs

def GenerateSchema(generator,
                   filenames,
                   root,
                   destdir,
                   root_namespace,
                   dart_overrides_dir):
  # Merge the source files into a single list of schemas.
  api_defs = []
  for filename in filenames:
    schema = os.path.normpath(filename)
    schema_filename, schema_extension = os.path.splitext(schema)
    path, short_filename = os.path.split(schema_filename)
    api_def = _LoadSchema(schema)

    # If compiling the C++ model code, delete 'nocompile' nodes.
    if generator == 'cpp':
      api_def = json_schema.DeleteNodes(api_def, 'nocompile')
    api_defs.extend(api_def)

  api_model = Model()

  # Load type dependencies into the model.
  #
  # HACK(kalman): bundle mode doesn't work with dependencies, because not all
  # schemas work in bundle mode.
  #
  # TODO(kalman): load dependencies lazily (get rid of the 'dependencies' list)
  # and this problem will go away.
  if generator != 'cpp-bundle':
    for target_namespace in api_defs:
      for referenced_schema in target_namespace.get('dependencies', []):
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
          api_model.AddNamespace(
              namespace,
              os.path.relpath(referenced_schema_path, root),
              include_compiler_options=True)

  # For single-schema compilation make sure that the first (i.e. only) schema
  # is the default one.
  default_namespace = None

  # Load the actual namespaces into the model.
  for target_namespace, schema_filename in zip(api_defs, filenames):
    relpath = os.path.relpath(os.path.normpath(schema_filename), root)
    namespace = api_model.AddNamespace(target_namespace,
                                       relpath,
                                       include_compiler_options=True)
    if default_namespace is None:
      default_namespace = namespace

    path, filename = os.path.split(schema_filename)
    short_filename, extension = os.path.splitext(filename)

    # Filenames are checked against the unix_names of the namespaces they
    # generate because the gyp uses the names of the JSON files to generate
    # the names of the .cc and .h files. We want these to be using unix_names.
    if namespace.unix_name != short_filename:
      sys.exit("Filename %s is illegal. Name files using unix_hacker style." %
               schema_filename)

    # The output filename must match the input filename for gyp to deal with it
    # properly.
    out_file = namespace.unix_name

  # Construct the type generator with all the namespaces in this model.
  type_generator = CppTypeGenerator(api_model,
                                    default_namespace=default_namespace)

  if generator == 'cpp-bundle':
    cpp_bundle_generator = CppBundleGenerator(root,
                                              api_model,
                                              api_defs,
                                              type_generator,
                                              root_namespace)
    generators = [
      ('generated_api.cc', cpp_bundle_generator.api_cc_generator),
      ('generated_api.h', cpp_bundle_generator.api_h_generator),
      ('generated_schemas.cc', cpp_bundle_generator.schemas_cc_generator),
      ('generated_schemas.h', cpp_bundle_generator.schemas_h_generator)
    ]
  elif generator == 'cpp':
    cpp_generator = CppGenerator(type_generator, root_namespace)
    generators = [
      ('%s.h' % namespace.unix_name, cpp_generator.h_generator),
      ('%s.cc' % namespace.unix_name, cpp_generator.cc_generator)
    ]
  elif generator == 'dart':
    generators = [
      ('%s.dart' % namespace.unix_name, DartGenerator(
          dart_overrides_dir))
    ]
  else:
    raise Exception('Unrecognised generator %s' % generator)

  output_code = []
  for filename, generator in generators:
    code = generator.Generate(namespace).Render()
    if destdir:
      with open(os.path.join(destdir, namespace.source_file_dir,
          filename), 'w') as f:
        f.write(code)
    output_code += [filename, '', code, '']

  return '\n'.join(output_code)

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
  parser.add_option('-g', '--generator', default=GENERATORS[0],
      choices=GENERATORS,
      help='The generator to use to build the output code. Supported values are'
      ' %s' % GENERATORS)
  parser.add_option('-D', '--dart-overrides-dir', dest='dart_overrides_dir',
      help='Adds custom dart from files in the given directory (Dart only).')

  (opts, filenames) = parser.parse_args()

  if not filenames:
    sys.exit(0) # This is OK as a no-op

  # Unless in bundle mode, only one file should be specified.
  if opts.generator != 'cpp-bundle' and len(filenames) > 1:
    # TODO(sashab): Could also just use filenames[0] here and not complain.
    raise Exception(
        "Unless in bundle mode, only one file can be specified at a time.")

  result = GenerateSchema(opts.generator, filenames, opts.root, opts.destdir,
                          opts.namespace, opts.dart_overrides_dir)
  if not opts.destdir:
    print result
