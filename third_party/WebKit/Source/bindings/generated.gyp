#
# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

# Generate IDL bindings, together with auxiliary files
# (constructors on global objects, aggregate bindings files).
#
# Design doc: http://www.chromium.org/developers/design-documents/idl-build

{
  'includes': [
    'bindings.gypi',
    'idl.gypi',
    'scripts/scripts.gypi',
    'templates/templates.gypi',
  ],

  'targets': [
################################################################################
  {
    # FIXME: Global constructors are used by bindings_core (e.g., V8Window.cpp)
    # but depend on modules, which violates layering http://crbug.com/358074
    'target_name': 'global_constructors_idls',
    'type': 'none',
    'actions': [{
      'action_name': 'generate_global_constructors_idls',
      'inputs': [
        'scripts/generate_global_constructors.py',
        'scripts/utilities.py',
        # Only includes main IDL files (exclude dependencies and testing,
        # which should not appear on global objects).
        '<(main_interface_idl_files_list)',
        '<@(main_interface_idl_files)',
      ],
      'outputs': [
        '<@(generated_global_constructors_idl_files)',
        '<@(generated_global_constructors_header_files)',
      ],
      'action': [
        'python',
        'scripts/generate_global_constructors.py',
        '--idl-files-list',
        '<(main_interface_idl_files_list)',
        '--write-file-only-if-changed',
        '<(write_file_only_if_changed)',
        '--',
        'Window',
        '<(blink_output_dir)/WindowConstructors.idl',
        'SharedWorkerGlobalScope',
        '<(blink_output_dir)/SharedWorkerGlobalScopeConstructors.idl',
        'DedicatedWorkerGlobalScope',
        '<(blink_output_dir)/DedicatedWorkerGlobalScopeConstructors.idl',
        'ServiceWorkerGlobalScope',
        '<(blink_output_dir)/ServiceWorkerGlobalScopeConstructors.idl',
       ],
       'message': 'Generating IDL files for constructors on global objects',
      }]
  },
################################################################################
  {
    # FIXME: interfaces_info is used by bindings_core, but depend on modules,
    # which violates layering http://crbug.com/358074
    # Generate separate interfaces_info_core and interfaces_info_modules
    'target_name': 'interfaces_info',
    'type': 'none',
    'dependencies': [
      # Generated IDLs
      'global_constructors_idls',
      '../core/core_generated.gyp:generated_testing_idls',
    ],
    'actions': [{
      'action_name': 'compute_interfaces_info',
      'inputs': [
        'scripts/compute_interfaces_info.py',
        'scripts/utilities.py',
        '<(static_idl_files_list)',
        '<@(static_idl_files)',
        '<@(generated_idl_files)',
      ],
      'outputs': [
        '<(blink_output_dir)/InterfacesInfo.pickle',
      ],
      'action': [
        'python',
        'scripts/compute_interfaces_info.py',
        '--idl-files-list',
        '<(static_idl_files_list)',
        '--interfaces-info-file',
        '<(blink_output_dir)/InterfacesInfo.pickle',
        '--write-file-only-if-changed',
        '<(write_file_only_if_changed)',
        '--',
        # Generated files must be passed at command line
        '<@(generated_idl_files)',
      ],
      'message': 'Computing global information about IDL files',
      }]
  },
################################################################################
  ],  # targets
}
