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
    '../WebKit/chromium/WinPrecompile.gypi',
    '../core/features.gypi',
    '../core/core.gypi',
    '../modules/modules.gypi',
    'bindings.gypi',
  ],

  'variables': {
    'idl_files': [
      '<@(core_idl_files)',
      '<@(modules_idl_files)',
      '<@(svg_idl_files)',
    ],

    'conditions': [
      ['OS=="win" and buildtype=="Official"', {
        # On windows official release builds, we try to preserve symbol space.
        'derived_sources_aggregate_files': [
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSourcesAll.cpp',
        ],
      },{
        'derived_sources_aggregate_files': [
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources01.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources02.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources03.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources04.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources05.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources06.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources07.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources08.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources09.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources10.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources11.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources12.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources13.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources14.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources15.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources16.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources17.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources18.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources19.cpp',
        ],
      }],
      # The bindings generator can not write generated files if they are identical
      # to the already existing file – that way they don't need to be recompiled.
      # However, a reverse dependency having a newer timestamp than a
      # generated binding can confuse some build systems, so only use this on
      # ninja which explicitly supports this use case (gyp turns all actions into
      # ninja restat rules).
      ['"<(GENERATOR)"=="ninja"', {
        'write_file_only_if_changed': '--write-file-only-if-changed 1',
      },{
        'write_file_only_if_changed': '--write-file-only-if-changed 0',
      }],
    ],
  },

  'target_defaults': {
    'variables': {
      'optimize': 'max',
    },
  },

  'targets': [{
    'target_name': 'supplemental_dependencies',
    'type': 'none',
    'actions': [{
      'action_name': 'generatePartialInterfacesDependency',
      'variables': {
        # Write sources into a file, so that the action command line won't
        # exceed OS limits.
        'idl_files_list': '<|(idl_files_list.tmp <@(idl_files))',
      },
      'inputs': [
        'scripts/preprocess_idls.py',
        '<(idl_files_list)',
        '<!@(cat <(idl_files_list))',
       ],
       'outputs': [
         '<(SHARED_INTERMEDIATE_DIR)/supplemental_dependency.tmp',
         '<(SHARED_INTERMEDIATE_DIR)/WindowConstructors.idl',
         '<(SHARED_INTERMEDIATE_DIR)/WorkerContextConstructors.idl',
       ],
       'msvs_cygwin_shell': 0,
       'action': [
         'python',
         'scripts/preprocess_idls.py',
         '--idl-files-list',
         '<(idl_files_list)',
         '--supplemental-dependency-file',
         '<(SHARED_INTERMEDIATE_DIR)/supplemental_dependency.tmp',
         '--window-constructors-file',
         '<(SHARED_INTERMEDIATE_DIR)/WindowConstructors.idl',
         '--workercontext-constructors-file',
         '<(SHARED_INTERMEDIATE_DIR)/WorkerContextConstructors.idl',
         '<@(write_file_only_if_changed)',
       ],
       'message': 'Resolving partial interfaces dependencies in all IDL files',
      }]
    },
    {
      'target_name': 'bindings_derived_sources',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'supplemental_dependencies',
        '../core/core_derived_sources.gyp:generate_test_support_idls',
      ],
      'sources': [
        '<@(idl_files)',
        '<@(webcore_test_support_idl_files)',
      ],
      'actions': [{
        'action_name': 'derived_sources_all_in_one',
        'inputs': [
          '../core/scripts/action_derivedsourcesallinone.py',
          '<(SHARED_INTERMEDIATE_DIR)/supplemental_dependency.tmp',
        ],
        'outputs': [
          '<@(derived_sources_aggregate_files)',
        ],
        'action': [
          'python',
          '../core/scripts/action_derivedsourcesallinone.py',
          '<(SHARED_INTERMEDIATE_DIR)/supplemental_dependency.tmp',
          '--',
          '<@(derived_sources_aggregate_files)',
        ],
      }],
      'rules': [{
        'rule_name': 'binding',
        'extension': 'idl',
        'msvs_external_rule': 1,
        'inputs': [
          'scripts/generate-bindings.pl',
          'scripts/CodeGeneratorV8.pm',
          'scripts/IDLParser.pm',
          'scripts/IDLSerializer.pm',
          'scripts/IDLAttributes.txt',
          '../core/scripts/preprocessor.pm',
          '<!@pymod_do_main(supplemental_idl_files <@(idl_files))',
          '<(SHARED_INTERMEDIATE_DIR)/WindowConstructors.idl',
          '<(SHARED_INTERMEDIATE_DIR)/WorkerContextConstructors.idl',
        ],
        'outputs': [
          # FIXME:  The .cpp file should be in webkit/bindings once
          # we coax GYP into supporting it (see 'action' below).
          '<(SHARED_INTERMEDIATE_DIR)/webcore/bindings/V8<(RULE_INPUT_ROOT).cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8<(RULE_INPUT_ROOT).h',
        ],
        'variables': {
          # IDL include paths. The generator will search recursively for IDL
          # files under these locations.
          'generator_include_dirs': [
            '--include', '../modules',
            '--include', '../core',
            '--include', '<(SHARED_INTERMEDIATE_DIR)/webkit',
          ],
          # Hook for embedders to specify extra directories to find IDL files.
          'extra_blink_generator_include_dirs%': [],
        },
        'msvs_cygwin_shell': 0,
        # FIXME:  Note that we put the .cpp files in webcore/bindings
        # but the .h files in webkit/bindings.  This is to work around
        # the unfortunate fact that GYP strips duplicate arguments
        # from lists.  When we have a better GYP way to suppress that
        # behavior, change the output location.
        'action': [
          '<(perl_exe)',
          '-w',
          '-Iscripts',
          '-I../core/scripts',
          '-I../../../JSON/out/lib/perl5',
          'scripts/generate-bindings.pl',
          '--outputHeadersDir',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings',
          '--outputDir',
          '<(SHARED_INTERMEDIATE_DIR)/webcore/bindings',
          '--idlAttributesFile',
          'scripts/IDLAttributes.txt',
          '--defines',
          '<(feature_defines)',
          '<@(generator_include_dirs)',
          '<@(extra_blink_generator_include_dirs)',
          '--supplementalDependencyFile',
          '<(SHARED_INTERMEDIATE_DIR)/supplemental_dependency.tmp',
          '--additionalIdlFiles',
          '<(webcore_test_support_idl_files)',
          '<(RULE_INPUT_PATH)',
          '<@(preprocessor)',
          '<@(write_file_only_if_changed)',
        ],
        'message': 'Generating binding from <(RULE_INPUT_PATH)',
      }],
    },
  ],
}
