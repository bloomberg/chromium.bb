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
    '../build/scripts/scripts.gypi',
    '../build/win/precompile.gypi',
    '../build/scripts/scripts.gypi',
    '../core/core.gypi',
    '../modules/modules.gypi',
    'bindings.gypi',
  ],

  'variables': {
    # For details, see: http://www.chromium.org/developers/web-idl-interfaces
    #
    # Interface IDL files / Dependency IDL files
    # Interface IDL files: generate individual bindings (includes testing)
    'interface_idl_files': [
      '<@(static_interface_idl_files)',
      '<@(generated_interface_idl_files)',
    ],
    # Dependency IDL files: don't generate individual bindings, but do process
    # in IDL dependency computation, and count as build dependencies
    'dependency_idl_files': [
      '<@(static_dependency_idl_files)',
      '<@(generated_dependency_idl_files)',
    ],
    # Main interface IDL files (excluding dependencies and testing)
    # are included as properties on global objects, and in aggregate bindings
    'main_interface_idl_files': [
      '<@(core_idl_files)',
      '<@(modules_idl_files)',
    ],

    # Static IDL files / Generated IDL files
    # Paths need to be passed separately for static and generated files, as
    # static files are listed in a temporary file (b/c too long for command
    # line), but generated files must be passed at the command line, as they are
    # not present at GYP time, when the temporary file is generated
    'static_idl_files': [
      '<@(static_interface_idl_files)',
      '<@(static_dependency_idl_files)',
    ],
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

    'generated_global_constructors_idl_files': [
      '<(SHARED_INTERMEDIATE_DIR)/blink/WindowConstructors.idl',
      '<(SHARED_INTERMEDIATE_DIR)/blink/WorkerGlobalScopeConstructors.idl',
      '<(SHARED_INTERMEDIATE_DIR)/blink/SharedWorkerGlobalScopeConstructors.idl',
      '<(SHARED_INTERMEDIATE_DIR)/blink/DedicatedWorkerGlobalScopeConstructors.idl',
      '<(SHARED_INTERMEDIATE_DIR)/ServiceWorkerGlobalScopeConstructors.idl',
    ],

    # Python source
    'jinja_module_files': [
      # jinja2/__init__.py contains version string, so sufficient for package
      '<(DEPTH)/third_party/jinja2/__init__.py',
      '<(DEPTH)/third_party/markupsafe/__init__.py',  # jinja2 dep
    ],
    'idl_compiler_files': [
      'scripts/idl_compiler.py',
      # PLY (Python Lex-Yacc)
      '<(DEPTH)/third_party/ply/lex.py',
      '<(DEPTH)/third_party/ply/yacc.py',
      # Web IDL lexer/parser (base parser)
      '<(DEPTH)/tools/idl_parser/idl_lexer.py',
      '<(DEPTH)/tools/idl_parser/idl_node.py',
      '<(DEPTH)/tools/idl_parser/idl_parser.py',
      # Blink IDL lexer/parser/constructor
      'scripts/blink_idl_lexer.py',
      'scripts/blink_idl_parser.py',
      'scripts/idl_definitions.py',
      'scripts/idl_definitions_builder.py',
      'scripts/idl_reader.py',
      'scripts/idl_validator.py',
      'scripts/interface_dependency_resolver.py',
      # V8 code generator
      'scripts/code_generator_v8.py',
      'scripts/v8_attributes.py',
      'scripts/v8_callback_interface.py',
      'scripts/v8_globals.py',
      'scripts/v8_interface.py',
      'scripts/v8_methods.py',
      'scripts/v8_types.py',
      'scripts/v8_utilities.py',
    ],

    # Jinja templates
    'code_generator_template_files': [
      'templates/attributes.cpp',
      'templates/callback_interface.cpp',
      'templates/callback_interface.h',
      'templates/interface_base.cpp',
      'templates/interface.cpp',
      'templates/interface.h',
      'templates/methods.cpp',
    ],

    'bindings_output_dir': '<(SHARED_INTERMEDIATE_DIR)/blink/bindings',

    'conditions': [
      # The bindings generator can skip writing generated files if they are
      # identical to the already existing file, which avoids recompilation.
      # However, a dependency (earlier build step) having a newer timestamp than
      # an output (later build step) confuses some build systems, so only use
      # this on ninja, which explicitly supports this use case (gyp turns all
      # actions into ninja restat rules).
      ['"<(GENERATOR)"=="ninja"', {
        'write_file_only_if_changed': '--write-file-only-if-changed 1',
      }, {
        'write_file_only_if_changed': '--write-file-only-if-changed 0',
      }],
    ],
  },

  'targets': [{
    'target_name': 'global_constructors_idls',
    'type': 'none',
    'actions': [{
      'action_name': 'generate_global_constructors_idls',
      'variables': {
        # Write list of IDL files to a file, so that the command line doesn't
        # exceed OS length limits.
        # Only includes main IDL files (exclude dependencies and testing,
        # which should not appear on global objects).
        'main_interface_idl_files_list': '<|(main_interface_idl_files_list.tmp <@(main_interface_idl_files))',
      },
      'inputs': [
        'scripts/generate_global_constructors.py',
        'scripts/utilities.py',
        '<(main_interface_idl_files_list)',
        '<@(main_interface_idl_files)',
      ],
      'outputs': [
        '<@(generated_global_constructors_idl_files)',
      ],
      'action': [
        'python',
        'scripts/generate_global_constructors.py',
        '--idl-files-list',
        '<(main_interface_idl_files_list)',
        '<@(write_file_only_if_changed)',
        '--',
        'Window',
        '<(SHARED_INTERMEDIATE_DIR)/blink/WindowConstructors.idl',
        'WorkerGlobalScope',
        '<(SHARED_INTERMEDIATE_DIR)/blink/WorkerGlobalScopeConstructors.idl',
        'SharedWorkerGlobalScope',
        '<(SHARED_INTERMEDIATE_DIR)/blink/SharedWorkerGlobalScopeConstructors.idl',
        'DedicatedWorkerGlobalScope',
        '<(SHARED_INTERMEDIATE_DIR)/blink/DedicatedWorkerGlobalScopeConstructors.idl',
        'ServiceWorkerGlobalScope',
        '<(SHARED_INTERMEDIATE_DIR)/ServiceWorkerGlobalScopeConstructors.idl',
       ],
       'message': 'Generating IDL files for constructors on global objects',
      }]
  },
  {
    'target_name': 'interfaces_info',
    'type': 'none',
    'dependencies': [
      'global_constructors_idls',
      '../core/core_generated.gyp:generated_testing_idls',
    ],
    'actions': [{
      'action_name': 'compute_interfaces_info',
      'variables': {
        # Write list of static IDL files to a file, so that the command line
        # doesn't exceed OS length limits.
        # Generated IDL files cannot be included, as their path depends on the
        # build directory, and must instead be passed as command line arguments.
        'idl_files_list': '<|(idl_files_list.tmp <@(static_idl_files))',
      },
      'inputs': [
        'scripts/compute_interfaces_info.py',
        'scripts/utilities.py',
        '<(idl_files_list)',
        '<@(static_idl_files)',
        '<@(generated_idl_files)',
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/blink/InterfacesInfo.pickle',
        '<(SHARED_INTERMEDIATE_DIR)/blink/EventInterfaces.in',
      ],
      'action': [
        'python',
        'scripts/compute_interfaces_info.py',
        '--idl-files-list',
        '<(idl_files_list)',
        '--interfaces-info-file',
        '<(SHARED_INTERMEDIATE_DIR)/blink/InterfacesInfo.pickle',
        '--event-names-file',
        '<(SHARED_INTERMEDIATE_DIR)/blink/EventInterfaces.in',
        '<@(write_file_only_if_changed)',
        '--',
        '<@(generated_idl_files)',
      ],
      'message': 'Computing global information about IDL files, and generating list of Event interfaces',
      }]
    },
    {
      # A separate pre-caching step is *required* to use bytecode caching in
      # Jinja (which improves speed significantly), as the bytecode cache is
      # not concurrency-safe on write; details in code_generator_v8.py.
      'target_name': 'cached_jinja_templates',
      'type': 'none',
      'actions': [{
        'action_name': 'cache_jinja_templates',
        'inputs': [
          '<@(jinja_module_files)',
          'scripts/code_generator_v8.py',
          '<@(code_generator_template_files)',
        ],
        'outputs': [
          '<(bindings_output_dir)/cached_jinja_templates.stamp',  # Dummy to track dependency
        ],
        'action': [
          'python',
          'scripts/code_generator_v8.py',
          '<(bindings_output_dir)',
          '<(bindings_output_dir)/cached_jinja_templates.stamp',
        ],
        'message': 'Caching bytecode of Jinja templates',
      }],
    },
    {
      'target_name': 'individual_generated_bindings',
      'type': 'none',
      # The 'binding' rule generates .h files, so mark as hard_dependency, per:
      # https://code.google.com/p/gyp/wiki/InputFormatReference#Linking_Dependencies
      'hard_dependency': 1,
      'dependencies': [
        'interfaces_info',
        'cached_jinja_templates',
        '../core/core_generated.gyp:generated_testing_idls',
      ],
      'sources': [
        '<@(interface_idl_files)',
      ],
      'rules': [{
        'rule_name': 'binding',
        'extension': 'idl',
        'msvs_external_rule': 1,
        'inputs': [
          '<@(idl_compiler_files)',
          '<(bindings_output_dir)/cached_jinja_templates.stamp',
          'IDLExtendedAttributes.txt',
          # If the dependency structure or public interface info (e.g.,
          # [ImplementedAs]) changes, we rebuild all files, since we're not
          # computing dependencies file-by-file in the build.
          # This data is generally stable.
          '<(SHARED_INTERMEDIATE_DIR)/blink/InterfacesInfo.pickle',
          # Further, if any dependency (partial interface or implemented
          # interface) changes, rebuild everything, since every IDL potentially
          # depends on them, because we're not computing dependencies
          # file-by-file.
          # FIXME: This is too conservative, and causes excess rebuilds:
          # compute this file-by-file.  http://crbug.com/341748
          '<@(dependency_idl_files)',
        ],
        'outputs': [
          '<(bindings_output_dir)/V8<(RULE_INPUT_ROOT).cpp',
          '<(bindings_output_dir)/V8<(RULE_INPUT_ROOT).h',
        ],
        # sanitize-win-build-log.sed uses a regex which matches this command
        # line (Python script + .idl file being processed).
        # Update that regex if command line changes (other than changing flags)
        'action': [
          'python',
          'scripts/idl_compiler.py',
          '--output-dir',
          '<(bindings_output_dir)',
          '--idl-attributes-file',
          'IDLExtendedAttributes.txt',
          '--interfaces-info',
          '<(SHARED_INTERMEDIATE_DIR)/blink/InterfacesInfo.pickle',
          '<@(write_file_only_if_changed)',
          '<(RULE_INPUT_PATH)',
        ],
        'message': 'Generating binding from <(RULE_INPUT_PATH)',
      }],
    },
    {
      'target_name': 'aggregate_generated_bindings',
      'type': 'none',
      'actions': [{
        'action_name': 'generate_aggregate_generated_bindings',
        'variables': {
          # Write list of IDL files to a file, so that the command line doesn't
          # exceed OS length limits.
          'main_interface_idl_files_list': '<|(main_interface_idl_files_list.tmp <@(main_interface_idl_files))',
          },
        'inputs': [
          'scripts/aggregate_generated_bindings.py',
          '<(main_interface_idl_files_list)',
        ],
        'outputs': [
          '<@(aggregate_generated_bindings_files)',
        ],
        'action': [
          'python',
          'scripts/aggregate_generated_bindings.py',
          '<(main_interface_idl_files_list)',
          '--',
          '<@(aggregate_generated_bindings_files)',
        ],
        'message': 'Generating aggregate generated bindings files',
      }],
    },
    {
      'target_name': 'generated_bindings',
      'type': 'none',
      'dependencies': [
        'aggregate_generated_bindings',
        'individual_generated_bindings',
      ],
    },
  ],
}
