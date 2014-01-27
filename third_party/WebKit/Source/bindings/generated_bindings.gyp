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
    # Generate individual bindings (include testing, exclude dependencies)
    'interface_idl_files': [
      '<@(core_idl_files)',
      '<@(webcore_testing_idl_files)',
      '<@(modules_idl_files)',
    ],
    # Dependencies
    'dependency_idl_files': [
      '<@(core_dependency_idl_files)',
      '<@(modules_dependency_idl_files)',
      '<@(modules_testing_dependency_idl_files)',
    ],
    # Include in aggregate bindings (exclude testing and dependencies)
    'main_interface_idl_files': [
      '<@(core_idl_files)',
      '<@(modules_idl_files)',
    ],

    # Generated IDL files
    # Dependencies need to be computed separately, as not present at GYP time
    # Generated interfaces need individual bindings generated
    'generated_interface_idl_files': [
      '<@(generated_webcore_testing_idl_files)',
    ],
    # Generated global constructors are partial interfaces, so no bindings
    'generated_global_constructors_idl_files': [
         '<(SHARED_INTERMEDIATE_DIR)/blink/WindowConstructors.idl',
         '<(SHARED_INTERMEDIATE_DIR)/blink/WorkerGlobalScopeConstructors.idl',
         '<(SHARED_INTERMEDIATE_DIR)/blink/SharedWorkerGlobalScopeConstructors.idl',
         '<(SHARED_INTERMEDIATE_DIR)/blink/DedicatedWorkerGlobalScopeConstructors.idl',
         '<(SHARED_INTERMEDIATE_DIR)/ServiceWorkerGlobalScopeConstructors.idl',
    ],

    'compiler_module_files': [
        'scripts/idl_compiler.py',
        '<(DEPTH)/third_party/ply/lex.py',
        '<(DEPTH)/third_party/ply/yacc.py',
        # jinja2/__init__.py contains version string, so sufficient for package
        '<(DEPTH)/third_party/jinja2/__init__.py',
        '<(DEPTH)/third_party/markupsafe/__init__.py',  # jinja2 dep
        '<(DEPTH)/tools/idl_parser/idl_lexer.py',
        '<(DEPTH)/tools/idl_parser/idl_node.py',
        '<(DEPTH)/tools/idl_parser/idl_parser.py',
        'scripts/blink_idl_lexer.py',
        'scripts/blink_idl_parser.py',
        'scripts/code_generator_v8.py',
        'scripts/idl_definitions.py',
        'scripts/idl_definitions_builder.py',
        'scripts/idl_reader.py',
        'scripts/idl_validator.py',
        'scripts/interface_dependency_resolver.py',
        'scripts/v8_attributes.py',
        'scripts/v8_callback_interface.py',
        'scripts/v8_interface.py',
        'scripts/v8_types.py',
        'scripts/v8_utilities.py',
    ],
    'code_generator_template_files': [
        'templates/attributes.cpp',
        'templates/callback_interface.cpp',
        'templates/callback_interface.h',
        'templates/interface_base.cpp',
        'templates/interface.cpp',
        'templates/interface.h',
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
      ['OS!="win"', {
        # This fails to import on Windows (running native perl) because of a
        # dependency on JSON::XS (which is a separate module). It's necessary
        # on Mac and CrOS. It's not generally necessary on standard Linux, but
        # depending on what the user has locally it could be. So, don't use on
        # Windows is the simplest solution.
        'json_perl_module_include_path': '-I<(DEPTH)/third_party/JSON/out/lib/perl5',
      }, {
        'json_perl_module_include_path': '',
      }],
    ],
  },

  'target_defaults': {
    'variables': {
      'optimize': 'max',
    },
  },

  'targets': [{
    'target_name': 'interface_dependencies',
    'type': 'none',
    'actions': [{
      'action_name': 'compute_interface_dependencies',
      'variables': {
        # Write list of IDL files to a file, so that the command line doesn't
        # exceed OS length limits.
        'idl_files_list': '<|(idl_files_list.tmp <@(interface_idl_files) <@(dependency_idl_files))',
      },
      'inputs': [
        'scripts/compute_dependencies.py',
        '<(idl_files_list)',
        '<@(interface_idl_files)',
        '<@(dependency_idl_files)',
       ],
       'outputs': [
         '<(SHARED_INTERMEDIATE_DIR)/blink/InterfaceDependencies.txt',
         '<(SHARED_INTERMEDIATE_DIR)/blink/InterfacesInfo.pickle',
         '<@(generated_global_constructors_idl_files)',
         '<(SHARED_INTERMEDIATE_DIR)/blink/EventInterfaces.in',
       ],
       'action': [
         'python',
         'scripts/compute_dependencies.py',
         '--idl-files-list',
         '<(idl_files_list)',
         '--interface-dependencies-file',
         '<(SHARED_INTERMEDIATE_DIR)/blink/InterfaceDependencies.txt',
         '--interfaces-info-file',
         '<(SHARED_INTERMEDIATE_DIR)/blink/InterfacesInfo.pickle',
         '--window-constructors-file',
         '<(SHARED_INTERMEDIATE_DIR)/blink/WindowConstructors.idl',
         '--workerglobalscope-constructors-file',
         '<(SHARED_INTERMEDIATE_DIR)/blink/WorkerGlobalScopeConstructors.idl',
         '--sharedworkerglobalscope-constructors-file',
         '<(SHARED_INTERMEDIATE_DIR)/blink/SharedWorkerGlobalScopeConstructors.idl',
         '--dedicatedworkerglobalscope-constructors-file',
         '<(SHARED_INTERMEDIATE_DIR)/blink/DedicatedWorkerGlobalScopeConstructors.idl',
         '--serviceworkerglobalscope-constructors-file',
         '<(SHARED_INTERMEDIATE_DIR)/ServiceWorkerGlobalScopeConstructors.idl',
         '--event-names-file',
         '<(SHARED_INTERMEDIATE_DIR)/blink/EventInterfaces.in',
         '<@(write_file_only_if_changed)',
       ],
       'message': 'Computing dependencies between IDL files, and generating global scope constructor IDLs files and list of Event interfaces',
      }]
    },
    {
      'target_name': 'individual_generated_bindings',
      'type': 'none',
      # The 'binding' rule generates .h files, so mark as hard_dependency, per:
      # https://code.google.com/p/gyp/wiki/InputFormatReference#Linking_Dependencies
      'hard_dependency': 1,
      'dependencies': [
        'interface_dependencies',
        '../config.gyp:config',
        '../core/core_generated.gyp:generated_testing_idls',
      ],
      'sources': [
        '<@(interface_idl_files)',
        '<@(generated_interface_idl_files)',
      ],
      'rules': [{
        'rule_name': 'binding',
        'extension': 'idl',
        'msvs_external_rule': 1,
        'inputs': [
          'scripts/generate_bindings.pl',
          'scripts/code_generator_v8.pm',
          'scripts/idl_parser.pm',
          'scripts/idl_serializer.pm',
          '../build/scripts/preprocessor.pm',
          'IDLExtendedAttributes.txt',
          # If the dependency structure or public interface info (e.g.,
          # [ImplementedAs]) changes, we rebuild all files, since we're not
          # computing dependencies file-by-file in the build.
          # This data is generally stable.
          '<(SHARED_INTERMEDIATE_DIR)/blink/InterfaceDependencies.txt',
          '<(SHARED_INTERMEDIATE_DIR)/blink/InterfacesInfo.pickle',
          # Further, if any dependency (partial interface or implemented
          # interface) changes, rebuild everything, since every IDL potentially
          # depends on them, because we're not computing dependencies
          # file-by-file.
          # FIXME: This is too conservative, and causes excess rebuilds:
          # compute this file-by-file.
          '<@(dependency_idl_files)',
          # Generated global constructors IDLs are all partial interfaces,
          # hence everything potentially depends on them.
          '<@(generated_global_constructors_idl_files)',
        ],
        'outputs': [
          '<(bindings_output_dir)/V8<(RULE_INPUT_ROOT).cpp',
          '<(bindings_output_dir)/V8<(RULE_INPUT_ROOT).h',
        ],
        'variables': {
          # IDL include paths. The generator will search recursively for IDL
          # files under these locations.
          'generator_include_dirs': [
            '--include', '../core',
            '--include', '../modules',
            '--include', '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
          # Hook for embedders to specify extra directories to find IDL files.
          'extra_blink_generator_include_dirs%': [],
        },
        # sanitize-win-build-log.sed uses a regex which matches this command
        # line (Perl script + .idl file being processed).
        # Update that regex if command line changes (other than changing flags)
        'action': [
          '<(perl_exe)',
          '-w',
          '-Iscripts',
          '-I../build/scripts',
          '<@(json_perl_module_include_path)',
          'scripts/generate_bindings.pl',
          '--outputDir',
          '<(bindings_output_dir)',
          '--idlAttributesFile',
          'IDLExtendedAttributes.txt',
          '<@(generator_include_dirs)',
          '<@(extra_blink_generator_include_dirs)',
          '--interfaceDependenciesFile',
          '<(SHARED_INTERMEDIATE_DIR)/blink/InterfaceDependencies.txt',
          '<@(preprocessor)',
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
