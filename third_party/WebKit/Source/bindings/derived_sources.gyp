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
    ],

    'conditions': [
      ['enable_svg!=0', {
        'idl_files': [
          '<@(svg_idl_files)',
        ],
      }],
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
        'scripts/preprocess-idls.pl',
        '<(idl_files_list)',
        '<!@(cat <(idl_files_list))',
       ],
       'outputs': [
         '<(SHARED_INTERMEDIATE_DIR)/supplemental_dependency.tmp',
         '<(SHARED_INTERMEDIATE_DIR)/DOMWindowConstructors.idl',
       ],
       'msvs_cygwin_shell': 0,
       'action': [
         '<(perl_exe)',
         '-w',
         '-Iscripts',
         '-I../core/scripts',
         'scripts/preprocess-idls.pl',
         '--defines',
         '<(feature_defines)',
         '--idlFilesList',
         '<(idl_files_list)',
         '--supplementalDependencyFile',
         '<(SHARED_INTERMEDIATE_DIR)/supplemental_dependency.tmp',
         '--windowConstructorsFile',
         '<(SHARED_INTERMEDIATE_DIR)/DOMWindowConstructors.idl',
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
        '../core/core.gyp/core_derived_sources.gyp:generate_test_support_idls',
      ],
      'sources': [
        '<@(idl_files)',
        '<@(webcore_test_support_idl_files)',
      ],
      'actions': [{
        'action_name': 'derived_sources_all_in_one',
        'inputs': [
          '../core/core.gyp/scripts/action_derivedsourcesallinone.py',
          '<(SHARED_INTERMEDIATE_DIR)/supplemental_dependency.tmp',
        ],
        'outputs': [
          '<@(derived_sources_aggregate_files)',
        ],
        'action': [
          'python',
          '../core/core.gyp/scripts/action_derivedsourcesallinone.py',
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
          'scripts/IDLAttributes.txt',
          '../core/scripts/preprocessor.pm',
          '<!@pymod_do_main(supplemental_idl_files <@(idl_files))',
          '<(SHARED_INTERMEDIATE_DIR)/DOMWindowConstructors.idl',
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
          '--supplementalDependencyFile',
          '<(SHARED_INTERMEDIATE_DIR)/supplemental_dependency.tmp',
          '--additionalIdlFiles',
          '<(webcore_test_support_idl_files)',
          '<(RULE_INPUT_PATH)',
          '<@(preprocessor)',
        ],
        'message': 'Generating binding from <(RULE_INPUT_PATH)',
      }],
    },
  ],
}
