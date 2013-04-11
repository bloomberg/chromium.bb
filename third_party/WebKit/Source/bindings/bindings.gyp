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
    '../WebKit/chromium/features.gypi',
    '../WebCore/WebCore.gypi',
    '../modules/modules.gypi',
    'bindings.gypi',
  ],

  'variables': {
    # If set to 1, doesn't compile debug symbols into webcore reducing the
    # size of the binary and increasing the speed of gdb.  gcc only.
    'remove_webcore_debug_symbols%': 0,

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

  'conditions': [
    ['OS!="win" and remove_webcore_debug_symbols==1', {
      # Remove -g from all targets defined here.
      'target_defaults': {
        'cflags!': ['-g'],
      },
    }],
    ['os_posix==1 and OS!="mac" and gcc_version>=46', {
      'target_defaults': {
        # Disable warnings about c++0x compatibility, as some names (such as nullptr) conflict
        # with upcoming c++0x types.
        'cflags_cc': ['-Wno-c++0x-compat'],
      },
    }],
    ['OS=="linux" and target_arch=="arm"', {
      # Due to a bug in gcc arm, we get warnings about uninitialized timesNewRoman.unstatic.3258
      # and colorTransparent.unstatic.4879.
      'target_defaults': {
        'cflags': ['-Wno-uninitialized'],
      },
    }],
    ['clang==1', {
      'target_defaults': {
        'cflags': ['-Wglobal-constructors', '-Wunused-parameter'],
        'xcode_settings': {
          'WARNING_CFLAGS': ['-Wglobal-constructors', '-Wunused-parameter'],
        },
      },
    }],
  ],
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
      'target_name': 'bindings_sources',
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
          '<(bindings_dir)/scripts/generate-bindings.pl',
          '--outputHeadersDir',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings',
          '--outputDir',
          '<(SHARED_INTERMEDIATE_DIR)/webcore/bindings',
          '--idlAttributesFile',
          'scripts/IDLAttributes.txt',
          '--defines',
          '<(feature_defines)',
          '--generator',
          'V8',
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
    {
      'target_name': 'bindings',
      'type': 'static_library',
      'hard_dependency': 1,
      'dependencies': [
        'bindings_sources',
        '../WebCore/WebCore.gyp/WebCore.gyp:webcore_prerequisites',
        '../yarr/yarr.gyp:yarr',
        '../WTF/WTF.gyp/WTF.gyp:wtf',
        '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/iccjpeg/iccjpeg.gyp:iccjpeg',
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
        '<(DEPTH)/third_party/libxml/libxml.gyp:libxml',
        '<(DEPTH)/third_party/libxslt/libxslt.gyp:libxslt',
        '<(DEPTH)/third_party/libwebp/libwebp.gyp:libwebp',
        '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
        '<(DEPTH)/third_party/qcms/qcms.gyp:qcms',
        '<(DEPTH)/third_party/sqlite/sqlite.gyp:sqlite',
        '<(DEPTH)/third_party/v8-i18n/build/all.gyp:v8-i18n',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
        '<(libjpeg_gyp_path):libjpeg',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        # FIXME:  Remove <(SHARED_INTERMEDIATE_DIR)/webcore when we
        # can entice gyp into letting us put both the .cpp and .h
        # files in the same output directory.
        '<(SHARED_INTERMEDIATE_DIR)/webcore',
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/webkit',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings',
        ],
      },
      'xcode_settings': {
        # Some Mac-specific parts of WebKit won't compile without having this
        # prefix header injected.
        # FIXME: make this a first-class setting.
        'GCC_PREFIX_HEADER': '<(webcore_prefix_file)',
      },
      'sources': [
        '<@(derived_sources_aggregate_files)',
        '<@(bindings_files)',
      ],
      'conditions': [
        ['OS=="win" and component=="shared_library"', {
          'defines': [
            'USING_V8_SHARED',
          ],
        }],
        ['OS=="win"', {
          'defines': [
            '__PRETTY_FUNCTION__=__FUNCTION__',
          ],
          # This is needed because Event.h in this directory is blocked
          # by a system header on windows.
          'include_dirs++': ['../WebCore/dom'],
          'direct_dependent_settings': {
            'include_dirs+++': ['../WebCore/dom'],
          },
          # In generated bindings code: 'switch contains default but no case'.
          # Disable c4267 warnings until we fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4065, 4267 ],
        }],
        ['OS in ("linux", "android") and "WTF_USE_WEBAUDIO_IPP=1" in feature_defines', {
          'cflags': [
            '<!@(pkg-config --cflags-only-I ipp)',
          ],
        }],
      ],
    },
  ],
}
