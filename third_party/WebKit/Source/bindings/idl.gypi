# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# IDL file lists; see: http://www.chromium.org/developers/web-idl-interfaces

{
  'includes': [
    'bindings.gypi',
    '../core/core.gypi',
    '../modules/modules.gypi',
  ],

  'variables': {
    # Interface IDL files / Dependency IDL files
    # Dependency IDL files: don't generate individual bindings, but do process
    # in IDL dependency computation, and count as build dependencies
    'dependency_idl_files': [
      '<@(static_dependency_idl_files)',
      '<@(generated_dependency_idl_files)',
    ],

    # Main interface IDL files (excluding dependencies and testing)
    # are included as properties on global objects, and in aggregate bindings
    # FIXME: split into core vs. modules http://crbug.com/358074
    'main_interface_idl_files': [
      '<@(core_idl_files)',
      '<@(modules_idl_files)',
    ],
    # Write lists of main IDL files to a file, so that the command lines don't
    # exceed OS length limits.
    'main_interface_idl_files_list': '<|(main_interface_idl_files_list.tmp <@(main_interface_idl_files))',
    'core_idl_files_list': '<|(core_idl_files_list.tmp <@(core_idl_files))',
    'modules_idl_files_list': '<|(modules_idl_files_list.tmp <@(modules_idl_files))',

    # Static IDL files / Generated IDL files
    # Paths need to be passed separately for static and generated files, as
    # static files are listed in a temporary file (b/c too long for command
    # line), but generated files must be passed at the command line, as their
    # paths are not fixed at GYP time, when the temporary file is generated,
    # because their paths depend on the build directory, which varies.
    # FIXME: split into core vs. modules http://crbug.com/358074
    'static_idl_files': [
      '<@(static_interface_idl_files)',
      '<@(static_dependency_idl_files)',
    ],
    'static_idl_files_list': '<|(static_idl_files_list.tmp <@(static_idl_files))',
    'generated_idl_files': [
      '<@(generated_interface_idl_files)',
      '<@(generated_dependency_idl_files)',
    ],

    # Static IDL files
    'static_interface_idl_files': [
      '<@(core_idl_files)',
      '<@(webcore_testing_idl_files)',
      '<@(modules_idl_files)',
    ],
    'static_dependency_idl_files': [
      '<@(core_dependency_idl_files)',
      '<@(modules_dependency_idl_files)',
      '<@(modules_testing_dependency_idl_files)',
    ],

    # Generated IDL files
    'generated_interface_idl_files': [
      '<@(generated_webcore_testing_idl_files)',  # interfaces
    ],
    'generated_dependency_idl_files': [
      '<@(generated_global_constructors_idl_files)',  # partial interfaces
    ],

    # Global constructors
    'generated_global_constructors_idl_files': [
      '<(blink_output_dir)/WindowConstructors.idl',
      '<(blink_output_dir)/SharedWorkerGlobalScopeConstructors.idl',
      '<(blink_output_dir)/DedicatedWorkerGlobalScopeConstructors.idl',
      '<(blink_output_dir)/ServiceWorkerGlobalScopeConstructors.idl',
    ],

    'generated_global_constructors_header_files': [
      '<(blink_output_dir)/WindowConstructors.h',
      '<(blink_output_dir)/SharedWorkerGlobalScopeConstructors.h',
      '<(blink_output_dir)/DedicatedWorkerGlobalScopeConstructors.h',
      '<(blink_output_dir)/ServiceWorkerGlobalScopeConstructors.h',
    ],
  },
}
