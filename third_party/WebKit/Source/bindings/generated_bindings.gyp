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
    '../core/core.gypi',
    '../modules/modules.gypi',
  ],

  'variables': {
    # IDL file lists; see: http://www.chromium.org/developers/web-idl-interfaces
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

    'generated_global_constructors_idl_files': [
      '<(blink_output_dir)/WindowConstructors.idl',
      '<(blink_output_dir)/WorkerGlobalScopeConstructors.idl',
      '<(blink_output_dir)/SharedWorkerGlobalScopeConstructors.idl',
      '<(blink_output_dir)/DedicatedWorkerGlobalScopeConstructors.idl',
      '<(blink_output_dir)/ServiceWorkerGlobalScopeConstructors.idl',
    ],

    'generated_global_constructors_header_files': [
      '<(blink_output_dir)/WindowConstructors.h',
      '<(blink_output_dir)/WorkerGlobalScopeConstructors.h',
      '<(blink_output_dir)/SharedWorkerGlobalScopeConstructors.h',
      '<(blink_output_dir)/DedicatedWorkerGlobalScopeConstructors.h',
      '<(blink_output_dir)/ServiceWorkerGlobalScopeConstructors.h',
    ],


    # Python source
    'jinja_module_files': [
      # jinja2/__init__.py contains version string, so sufficient for package
      '<(DEPTH)/third_party/jinja2/__init__.py',
      '<(DEPTH)/third_party/markupsafe/__init__.py',  # jinja2 dep
    ],
    'idl_lexer_parser_files': [
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
    ],
    'idl_compiler_files': [
      'scripts/idl_compiler.py',
      # Blink IDL front end (ex-lexer/parser)
      'scripts/idl_definitions.py',
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
  },

  'targets': [
################################################################################
  {
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
        'WorkerGlobalScope',
        '<(blink_output_dir)/WorkerGlobalScopeConstructors.idl',
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
  {
    # A separate pre-caching step is *not required* to use lex/parse table
    # caching in PLY, as the caches are concurrency-safe.
    # However, pre-caching ensures that all compiler processes use the cached
    # files (hence maximizing speed), instead of early processes building the
    # tables themselves (as they've not yet been written when they start).
    'target_name': 'cached_lex_yacc_tables',
    'type': 'none',
    'actions': [{
      'action_name': 'cache_lex_yacc_tables',
      'inputs': [
        '<@(idl_lexer_parser_files)',
      ],
      'outputs': [
        '<(bindings_output_dir)/lextab.py',
        '<(bindings_output_dir)/parsetab.pickle',
      ],
      'action': [
        'python',
        'scripts/blink_idl_parser.py',
        '<(bindings_output_dir)',
      ],
      'message': 'Caching PLY lex & yacc lex/parse tables',
    }],
  },
################################################################################
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
################################################################################
  {
    'target_name': 'individual_generated_bindings',
    'type': 'none',
    # The 'binding' rule generates .h files, so mark as hard_dependency, per:
    # https://code.google.com/p/gyp/wiki/InputFormatReference#Linking_Dependencies
    'hard_dependency': 1,
    'dependencies': [
      'interfaces_info',
      'cached_lex_yacc_tables',
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
        '<@(idl_lexer_parser_files)',  # to be explicit (covered by parsetab)
        '<@(idl_compiler_files)',
        '<(bindings_output_dir)/lextab.py',
        '<(bindings_output_dir)/parsetab.pickle',
        '<(bindings_output_dir)/cached_jinja_templates.stamp',
        'IDLExtendedAttributes.txt',
        # If the dependency structure or public interface info (e.g.,
        # [ImplementedAs]) changes, we rebuild all files, since we're not
        # computing dependencies file-by-file in the build.
        # This data is generally stable.
        '<(blink_output_dir)/InterfacesInfo.pickle',
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
        '-S',  # skip 'import site' to speed up startup
        'scripts/idl_compiler.py',
        '--output-dir',
        '<(bindings_output_dir)',
        '--interfaces-info',
        '<(blink_output_dir)/InterfacesInfo.pickle',
        '--write-file-only-if-changed',
        '<(write_file_only_if_changed)',
        '<(RULE_INPUT_PATH)',
      ],
      'message': 'Generating binding from <(RULE_INPUT_PATH)',
    }],
  },
################################################################################
  {
    'target_name': 'bindings_core_generated_aggregate',
    'type': 'none',
    'actions': [{
      'action_name': 'generate_aggregate_bindings_core',
      'inputs': [
        'scripts/aggregate_generated_bindings.py',
        '<(core_idl_files_list)',
      ],
      'outputs': [
        '<@(bindings_core_generated_aggregate_files)',
      ],
      'action': [
        'python',
        'scripts/aggregate_generated_bindings.py',
        '<(core_idl_files_list)',
        '--',
        '<@(bindings_core_generated_aggregate_files)',
      ],
      'message': 'Generating aggregate generated core bindings files',
    }],
  },
################################################################################
  {
    'target_name': 'bindings_modules_generated_aggregate',
    'type': 'none',
    'actions': [{
      'action_name': 'generate_aggregate_bindings_modules',
      'inputs': [
        'scripts/aggregate_generated_bindings.py',
        '<(modules_idl_files_list)',
      ],
      'outputs': [
        '<@(bindings_modules_generated_aggregate_files)',
      ],
      'action': [
        'python',
        'scripts/aggregate_generated_bindings.py',
        '<(modules_idl_files_list)',
        '--',
        '<@(bindings_modules_generated_aggregate_files)',
      ],
      'message': 'Generating aggregate generated modules bindings files',
    }],
  },
################################################################################
  {
    'target_name': 'generated_bindings',
    'type': 'none',
    'dependencies': [
      'bindings_core_generated_aggregate',
      'bindings_modules_generated_aggregate',
      'individual_generated_bindings',
    ],
  },
################################################################################
  ],  # targets
}
