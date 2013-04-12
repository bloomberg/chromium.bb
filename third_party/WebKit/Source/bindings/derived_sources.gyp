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
    '../WebKit/chromium/features.gypi',
    '../WebCore/WebCore.gypi',
    '../modules/modules.gypi',
    '../bindings/bindings.gypi',
  ],

  'variables': {
    'bindings_idl_files': [
      '<@(webcore_idl_files)',
      '<@(modules_idl_files)',
    ],

    'bindings_idl_files!': [
      # Custom bindings in bindings/v8/custom exist for these.
      '../WebCore/dom/EventListener.idl',

      '../WebCore/page/AbstractView.idl',

      # These bindings are excluded, as they're only used through inheritance and don't define constants that would need a constructor.
      '../WebCore/svg/ElementTimeControl.idl',
      '../WebCore/svg/SVGExternalResourcesRequired.idl',
      '../WebCore/svg/SVGFilterPrimitiveStandardAttributes.idl',
      '../WebCore/svg/SVGFitToViewBox.idl',
      '../WebCore/svg/SVGLangSpace.idl',
      '../WebCore/svg/SVGLocatable.idl',
      '../WebCore/svg/SVGTests.idl',
      '../WebCore/svg/SVGTransformable.idl',

      # FIXME: I don't know why these are excluded, either.
      # Someone (me?) should figure it out and add appropriate comments.
      '../WebCore/css/CSSUnknownRule.idl',
    ],

    'conditions': [
      ['enable_svg!=0', {
        'bindings_idl_files': [
          '<@(webcore_svg_idl_files)',
        ],
      }],
    ],
  },

  'targets': [{
    'target_name': 'supplemental_dependencies',
    'type': 'none',
    'actions': [{
      'action_name': 'generateSupplementalDependency',
      'variables': {
        # Write sources into a file, so that the action command line won't
        # exceed OS limits.
        'idl_files_list': '<|(idl_files_list.tmp <@(bindings_idl_files))',
      },
      'inputs': [
        'scripts/preprocess-idls.pl',
        '<(idl_files_list)',
        '<!@(cat <(idl_files_list))',
       ],
       'outputs': [
         '<(SHARED_INTERMEDIATE_DIR)/supplemental_dependency.tmp',
       ],
       'msvs_cygwin_shell': 0,
       'action': [
         '<(perl_exe)',
         '-w',
         '-Iscripts',
         '-I../WebCore/scripts',
         'scripts/preprocess-idls.pl',
         '--defines',
         '<(feature_defines)',
         '--idlFilesList',
         '<(idl_files_list)',
         '--supplementalDependencyFile',
         '<(SHARED_INTERMEDIATE_DIR)/supplemental_dependency.tmp',
       ],
       'message': 'Resolving [Supplemental=XXX] dependencies in all IDL files',
      }]
    },
    {
      'target_name': 'bindings_derived_sources',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'supplemental_dependencies',
      ],
      'sources': [
        '<@(bindings_idl_files)',
        '<@(webcore_test_support_idl_files)',
      ],
      'actions': [{
        'action_name': 'derived_sources_all_in_one',
        'inputs': [
          '../WebCore/WebCore.gyp/scripts/action_derivedsourcesallinone.py',
          '<(SHARED_INTERMEDIATE_DIR)/supplemental_dependency.tmp',
        ],
        'outputs': [
          '<@(derived_sources_aggregate_files)',
        ],
        'action': [
          'python',
          '../WebCore/WebCore.gyp/scripts/action_derivedsourcesallinone.py',
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
          'scripts/CodeGenerator.pm',
          'scripts/CodeGeneratorV8.pm',
          'scripts/IDLParser.pm',
          'scripts/IDLAttributes.txt',
          '../WebCore/scripts/preprocessor.pm',
          '<!@pymod_do_main(supplemental_idl_files <@(bindings_idl_files))',
        ],
        'outputs': [
          # FIXME:  The .cpp file should be in webkit/bindings once
          # we coax GYP into supporting it (see 'action' below).
          '<(SHARED_INTERMEDIATE_DIR)/webcore/bindings/V8<(RULE_INPUT_ROOT).cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8<(RULE_INPUT_ROOT).h',
        ],
        'variables': {
          'generator_include_dirs': [
            '--include', '../modules/filesystem',
            '--include', '../modules/indexeddb',
            '--include', '../modules/mediasource',
            '--include', '../modules/mediastream',
            '--include', '../modules/navigatorcontentutils',
            '--include', '../modules/notifications',
            '--include', '../modules/webaudio',
            '--include', '../modules/webdatabase',
            '--include', '../WebCore/css',
            '--include', '../WebCore/dom',
            '--include', '../WebCore/fileapi',
            '--include', '../WebCore/html',
            '--include', '../WebCore/page',
            '--include', '../WebCore/plugins',
            '--include', '../WebCore/storage',
            '--include', '../WebCore/svg',
            '--include', '../WebCore/testing',
            '--include', '../WebCore/workers',
            '--include', '../WebCore/xml',
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
          '-I../WebCore/scripts',
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
