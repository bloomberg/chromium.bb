# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# IDL file lists; see: http://www.chromium.org/developers/web-idl-interfaces

{
  'includes': [
    '../../core/core.gypi',
    '../core/idl.gypi',
    '../idl.gypi',
  ],

  'variables': {
    # IDL file lists; see: http://www.chromium.org/developers/web-idl-interfaces

    # Dependency IDL files: don't generate individual bindings, but do process
    # in IDL dependency computation, and count as build dependencies
    'all_dependency_idl_files': [
      '<@(core_all_dependency_idl_files)',
      '<@(modules_all_dependency_idl_files)',
    ],
    # 'modules_dependency_idl_files' is already used in Source/modules, so avoid
    # collision
    'modules_all_dependency_idl_files': [
      '<@(modules_static_dependency_idl_files)',
      # '<@(modules_generated_dependency_idl_files)',
    ],

    # Static IDL files / Generated IDL files
    # Paths need to be passed separately for static and generated files, as
    # static files are listed in a temporary file (b/c too long for command
    # line), but generated files must be passed at the command line, as their
    # paths are not fixed at GYP time, when the temporary file is generated,
    # because their paths depend on the build directory, which varies.
    'modules_static_idl_files': [
      '<@(modules_static_interface_idl_files)',
      '<@(modules_static_dependency_idl_files)',
    ],
    'modules_static_idl_files_list':
      '<|(modules_static_idl_files_list.tmp <@(modules_static_idl_files))',

    #'modules_generated_idl_files': [
    #  '<@(modules_generated_dependency_idl_files)',
    #],

    # Static IDL files
    'modules_static_interface_idl_files': [
      '<@(modules_idl_files)',
    ],
    'modules_static_dependency_idl_files': [
      '<@(modules_dependency_idl_files)',
      '<@(modules_testing_dependency_idl_files)',
    ],

    # Generated IDL files
    #'modules_generated_dependency_idl_files': [
    #  # FIXME: Generate separate modules_global_constructors_idls
    #  # http://crbug.com/358074
    #  # '<@(modules_generated_global_constructors_idl_files)',  # partial interfaces
    #],
  },
}
