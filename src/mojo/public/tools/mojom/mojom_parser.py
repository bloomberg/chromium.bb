#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Parses mojom IDL files.

This script parses one or more input mojom files and produces corresponding
module files fully describing the definitions contained within each mojom. The
module data is pickled and can be easily consumed by other tools to, e.g.,
generate usable language bindings.
"""

import argparse
import errno
import json
import os
import os.path
import sys
from collections import defaultdict

from mojom.generate import module
from mojom.generate import translate
from mojom.parse import parser
from mojom.parse import conditional_features


def _ResolveRelativeImportPath(path, roots):
  """Attempts to resolve a relative import path against a set of possible roots.

  Args:
    path: The relative import path to resolve.
    roots: A list of absolute paths which will be checked in descending length
        order for a match against path.

  Returns:
    A normalized absolute path combining one of the roots with the input path if
    and only if such a file exists.

  Raises:
    ValueError: The path could not be resolved against any of the given roots.
  """
  for root in reversed(sorted(roots, key=len)):
    abs_path = os.path.join(root, path)
    if os.path.isfile(abs_path):
      return os.path.normcase(os.path.normpath(abs_path))

  raise ValueError('"%s" does not exist in any of %s' % (path, roots))


def _RebaseAbsolutePath(path, roots):
  """Rewrites an absolute file path as relative to an absolute directory path in
  roots.

  Args:
    path: The absolute path of an existing file.
    roots: A list of absolute directory paths. The given path argument must fall
        within one of these directories.

  Returns:
    A path equivalent to the input path, but relative to one of the provided
    roots. If the input path falls within multiple roots, the longest root is
    chosen (and thus the shortest relative path is returned).

    Paths returned by this method always use forward slashes as a separator to
    mirror mojom import syntax.

  Raises:
    ValueError if the given path does not fall within any of the listed roots.
  """
  assert os.path.isabs(path)
  assert os.path.isfile(path)
  assert all(map(os.path.isabs, roots))

  sorted_roots = list(reversed(sorted(roots, key=len)))

  def try_rebase_path(path, root):
    head, rebased_path = os.path.split(path)
    while head != root:
      head, tail = os.path.split(head)
      if not tail:
        return None
      rebased_path = os.path.join(tail, rebased_path)
    return rebased_path

  for root in sorted_roots:
    relative_path = try_rebase_path(path, root)
    if relative_path:
      # TODO(crbug.com/953884): Use pathlib for this kind of thing once we're
      # fully migrated to Python 3.
      return relative_path.replace('\\', '/')

  raise ValueError('%s does not fall within any of %s' % (path, sorted_roots))


def _GetModuleFilename(mojom_filename):
  return mojom_filename + '-module'


def _EnsureInputLoaded(mojom_abspath, module_path, abs_paths, asts,
                       dependencies, loaded_modules):
  """Recursively ensures that a module and its dependencies are loaded.

  Args:
    mojom_abspath: An absolute file path pointing to a mojom file to load.
    module_path: The relative path used to identify mojom_abspath.
    abs_paths: A mapping from module paths to absolute file paths for all
        inputs given to this execution of the script.
    asts: A map from each input mojom's absolute path to its parsed AST.
    dependencies: A mapping of which input mojoms depend on each other, indexed
        by absolute file path.
    loaded_modules: A mapping of all modules loaded so far, including non-input
        modules that were pulled in as transitive dependencies of the inputs.
    import_set: The working set of mojom imports processed so far in this
        call stack. Used to detect circular dependencies.
    import_stack: An ordered list of imports processed so far in this call
        stack. Used to report circular dependencies.

  Returns:
    None

    On return, loaded_modules will be populated with the loaded input mojom's
    Module as well as the Modules of all of its transitive dependencies."""

  if mojom_abspath in loaded_modules:
    # Already done.
    return

  for dep_abspath, dep_path in dependencies[mojom_abspath]:
    if dep_abspath not in loaded_modules:
      _EnsureInputLoaded(dep_abspath, dep_path, abs_paths, asts, dependencies,
                         loaded_modules)

  imports = {}
  for imp in asts[mojom_abspath].import_list:
    path = imp.import_filename
    imports[path] = loaded_modules[abs_paths[path]]
  loaded_modules[mojom_abspath] = translate.OrderedModule(
      asts[mojom_abspath], module_path, imports)


def _CollectAllowedImportsFromBuildMetadata(build_metadata_filename):
  allowed_imports = set()
  processed_deps = set()

  def collect(metadata_filename):
    processed_deps.add(metadata_filename)
    with open(metadata_filename) as f:
      metadata = json.load(f)
      allowed_imports.update(
          map(os.path.normcase, map(os.path.normpath, metadata['sources'])))
      for dep_metadata in metadata['deps']:
        if dep_metadata not in processed_deps:
          collect(dep_metadata)

  collect(build_metadata_filename)
  return allowed_imports


def _ParseMojoms(mojom_files,
                 input_root_paths,
                 output_root_path,
                 enabled_features,
                 allowed_imports=None):
  """Parses a set of mojom files and produces serialized module outputs.

  Args:
    mojom_files: A list of mojom files to process. Paths must be absolute paths
        which fall within one of the input or output root paths.
    input_root_paths: A list of absolute filesystem paths which may be used to
        resolve relative mojom file paths.
    output_root_path: An absolute filesystem path which will service as the root
        for all emitted artifacts. Artifacts produced from a given mojom file
        are based on the mojom's relative path, rebased onto this path.
        Additionally, the script expects this root to contain already-generated
        modules for any transitive dependencies not listed in mojom_files.
    enabled_features: A list of enabled feature names, controlling which AST
        nodes are filtered by [EnableIf] attributes.

  Returns:
    None.

    Upon completion, a mojom-module file will be saved for each input mojom.
  """
  assert input_root_paths
  assert output_root_path

  loaded_mojom_asts = {}
  loaded_modules = {}
  input_dependencies = defaultdict(set)
  mojom_files_to_parse = dict((os.path.normcase(abs_path),
                               _RebaseAbsolutePath(abs_path, input_root_paths))
                              for abs_path in mojom_files)
  abs_paths = dict(
      (path, abs_path) for abs_path, path in mojom_files_to_parse.items())
  for mojom_abspath, _ in mojom_files_to_parse.items():
    with open(mojom_abspath) as f:
      ast = parser.Parse(''.join(f.readlines()), mojom_abspath)
      conditional_features.RemoveDisabledDefinitions(ast, enabled_features)
      loaded_mojom_asts[mojom_abspath] = ast
      invalid_imports = []
      for imp in ast.import_list:
        import_abspath = _ResolveRelativeImportPath(imp.import_filename,
                                                    input_root_paths)
        if allowed_imports and import_abspath not in allowed_imports:
          invalid_imports.append(imp.import_filename)

        abs_paths[imp.import_filename] = import_abspath
        if import_abspath in mojom_files_to_parse:
          # This import is in the input list, so we're going to translate it
          # into a module below; however it's also a dependency of another input
          # module. We retain record of dependencies to help with input
          # processing later.
          input_dependencies[mojom_abspath].add((import_abspath,
                                                 imp.import_filename))
        else:
          # We have an import that isn't being parsed right now. It must already
          # be parsed and have a module file sitting in a corresponding output
          # location.
          module_path = _GetModuleFilename(imp.import_filename)
          module_abspath = _ResolveRelativeImportPath(module_path,
                                                      [output_root_path])
          with open(module_abspath, 'rb') as module_file:
            loaded_modules[import_abspath] = module.Module.Load(module_file)

      if invalid_imports:
        raise ValueError(
            '\nThe file %s imports the following files not allowed by build '
            'dependencies:\n\n%s\n' % (mojom_abspath,
                                       '\n'.join(invalid_imports)))


  # At this point all transitive imports not listed as inputs have been loaded
  # and we have a complete dependency tree of the unprocessed inputs. Now we can
  # load all the inputs, resolving dependencies among them recursively as we go.
  num_existing_modules_loaded = len(loaded_modules)
  for mojom_abspath, mojom_path in mojom_files_to_parse.items():
    _EnsureInputLoaded(mojom_abspath, mojom_path, abs_paths, loaded_mojom_asts,
                       input_dependencies, loaded_modules)
  assert (num_existing_modules_loaded +
          len(mojom_files_to_parse) == len(loaded_modules))

  # Now we have fully translated modules for every input and every transitive
  # dependency. We can dump the modules to disk for other tools to use.
  for mojom_abspath, mojom_path in mojom_files_to_parse.items():
    module_path = os.path.join(output_root_path, _GetModuleFilename(mojom_path))
    module_dir = os.path.dirname(module_path)
    if not os.path.exists(module_dir):
      try:
        # Python 2 doesn't support exist_ok on makedirs(), so we just ignore
        # that failure if it happens. It's possible during build due to races
        # among build steps with module outputs in the same directory.
        os.makedirs(module_dir)
      except OSError as e:
        if e.errno != errno.EEXIST:
          raise
    with open(module_path, 'wb') as f:
      loaded_modules[mojom_abspath].Dump(f)


def Run(command_line):
  arg_parser = argparse.ArgumentParser(
      description="""
Parses one or more mojom files and produces corresponding module outputs fully
describing the definitions therein. The output is exhaustive, stable, and
sufficient for another tool to consume and emit e.g. usable language
bindings based on the original mojoms.""",
      epilog="""
Note that each transitive import dependency reachable from the input mojoms must
either also be listed as an input or must have its corresponding compiled module
already present in the provided output root.""")

  arg_parser.add_argument(
      '--input-root',
      default=[],
      action='append',
      metavar='ROOT',
      dest='input_root_paths',
      help='Adds ROOT to the set of root paths against which relative input '
      'paths should be resolved. Provided root paths are always searched '
      'in order from longest absolute path to shortest.')
  arg_parser.add_argument(
      '--output-root',
      action='store',
      required=True,
      dest='output_root_path',
      metavar='ROOT',
      help='Use ROOT as the root path in which the parser should emit compiled '
      'modules for each processed input mojom. The path of emitted module is '
      'based on the relative input path, rebased onto this root. Note that '
      'ROOT is also searched for existing modules of any transitive imports '
      'which were not included in the set of inputs.')
  arg_parser.add_argument(
      '--mojoms',
      nargs='+',
      dest='mojom_files',
      default=[],
      metavar='MOJOM_FILE',
      help='Input mojom filename(s). Each filename must be either an absolute '
      'path which falls within one of the given input or output roots, or a '
      'relative path the parser will attempt to resolve using each of those '
      'roots in unspecified order.')
  arg_parser.add_argument(
      '--mojom-file-list',
      action='store',
      metavar='LIST_FILENAME',
      help='Input file whose contents are a list of mojoms to process. This '
      'may be provided in lieu of --mojoms to avoid hitting command line '
      'length limtations')
  arg_parser.add_argument(
      '--enable-feature',
      dest='enabled_features',
      default=[],
      action='append',
      metavar='FEATURE',
      help='Enables a named feature when parsing the given mojoms. Features '
      'are identified by arbitrary string values. Specifying this flag with a '
      'given FEATURE name will cause the parser to process any syntax elements '
      'tagged with an [EnableIf=FEATURE] attribute. If this flag is not '
      'provided for a given FEATURE, such tagged elements are discarded by the '
      'parser and will not be present in the compiled output.')
  arg_parser.add_argument(
      '--check-imports',
      dest='build_metadata_filename',
      action='store',
      metavar='METADATA_FILENAME',
      help='Instructs the parser to check imports against a set of allowed '
      'imports. Allowed imports are based on build metadata within '
      'METADATA_FILENAME. This is a JSON file with a `sources` key listing '
      'paths to the set of input mojom files being processed by this parser '
      'run, and a `deps` key listing paths to metadata files for any '
      'dependencies of these inputs. This feature can be used to implement '
      'build-time dependency checking for mojom imports, where each build '
      'metadata file corresponds to a build target in the dependency graph of '
      'a typical build system.')

  args, _ = arg_parser.parse_known_args(command_line)
  if args.mojom_file_list:
    with open(args.mojom_file_list) as f:
      args.mojom_files.extend(f.read().split())

  if not args.mojom_files:
    raise ValueError(
        'Must list at least one mojom file via --mojoms or --mojom-file-list')

  mojom_files = list(map(os.path.abspath, args.mojom_files))
  input_roots = list(map(os.path.abspath, args.input_root_paths))
  output_root = os.path.abspath(args.output_root_path)

  if args.build_metadata_filename:
    allowed_imports = _CollectAllowedImportsFromBuildMetadata(
        args.build_metadata_filename)
  else:
    allowed_imports = None

  _ParseMojoms(mojom_files, input_roots, output_root, args.enabled_features,
               allowed_imports)


if __name__ == '__main__':
  Run(sys.argv[1:])
