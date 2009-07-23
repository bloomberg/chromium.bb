# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'feature_defines': [
      'ENABLE_CHANNEL_MESSAGING=1',
      'ENABLE_DATABASE=1',
      'ENABLE_DATAGRID=1',
      'ENABLE_DASHBOARD_SUPPORT=0',
      'ENABLE_DOM_STORAGE=1',
      'ENABLE_JAVASCRIPT_DEBUGGER=0',
      'ENABLE_JSC_MULTIPLE_THREADS=0',
      'ENABLE_ICONDATABASE=0',
      'ENABLE_XSLT=1',
      'ENABLE_XPATH=1',
      'ENABLE_SHARED_WORKERS=0',
      'ENABLE_SVG=1',
      'ENABLE_SVG_ANIMATION=1',
      'ENABLE_SVG_AS_IMAGE=1',
      'ENABLE_SVG_USE=1',
      'ENABLE_SVG_FOREIGN_OBJECT=1',
      'ENABLE_SVG_FONTS=1',
      'ENABLE_VIDEO=1',
      'ENABLE_WORKERS=1',
    ],
    'non_feature_defines': [
      'BUILDING_CHROMIUM__=1',
      'USE_GOOGLE_URL_LIBRARY=1',
      'USE_SYSTEM_MALLOC=1',
    ],
    'webcore_include_dirs': [
      '../third_party/WebKit/WebCore/accessibility',
      '../third_party/WebKit/WebCore/accessibility/chromium',
      '../third_party/WebKit/WebCore/bindings/v8',
      '../third_party/WebKit/WebCore/bindings/v8/custom',
      '../third_party/WebKit/WebCore/css',
      '../third_party/WebKit/WebCore/dom',
      '../third_party/WebKit/WebCore/dom/default',
      '../third_party/WebKit/WebCore/editing',
      '../third_party/WebKit/WebCore/history',
      '../third_party/WebKit/WebCore/html',
      '../third_party/WebKit/WebCore/inspector',
      '../third_party/WebKit/WebCore/loader',
      '../third_party/WebKit/WebCore/loader/appcache',
      '../third_party/WebKit/WebCore/loader/archive',
      '../third_party/WebKit/WebCore/loader/icon',
      '../third_party/WebKit/WebCore/page',
      '../third_party/WebKit/WebCore/page/animation',
      '../third_party/WebKit/WebCore/page/chromium',
      '../third_party/WebKit/WebCore/platform',
      '../third_party/WebKit/WebCore/platform/animation',
      '../third_party/WebKit/WebCore/platform/chromium',
      '../third_party/WebKit/WebCore/platform/graphics',
      '../third_party/WebKit/WebCore/platform/graphics/chromium',
      '../third_party/WebKit/WebCore/platform/graphics/opentype',
      '../third_party/WebKit/WebCore/platform/graphics/skia',
      '../third_party/WebKit/WebCore/platform/graphics/transforms',
      '../third_party/WebKit/WebCore/platform/image-decoders',
      '../third_party/WebKit/WebCore/platform/image-decoders/bmp',
      '../third_party/WebKit/WebCore/platform/image-decoders/gif',
      '../third_party/WebKit/WebCore/platform/image-decoders/ico',
      '../third_party/WebKit/WebCore/platform/image-decoders/jpeg',
      '../third_party/WebKit/WebCore/platform/image-decoders/png',
      '../third_party/WebKit/WebCore/platform/image-decoders/skia',
      '../third_party/WebKit/WebCore/platform/image-decoders/xbm',
      '../third_party/WebKit/WebCore/platform/image-encoders/skia',
      '../third_party/WebKit/WebCore/platform/network',
      '../third_party/WebKit/WebCore/platform/network/chromium',
      '../third_party/WebKit/WebCore/platform/sql',
      '../third_party/WebKit/WebCore/platform/text',
      '../third_party/WebKit/WebCore/plugins',
      '../third_party/WebKit/WebCore/rendering',
      '../third_party/WebKit/WebCore/rendering/style',
      '../third_party/WebKit/WebCore/storage',
      '../third_party/WebKit/WebCore/svg',
      '../third_party/WebKit/WebCore/svg/animation',
      '../third_party/WebKit/WebCore/svg/graphics',
      '../third_party/WebKit/WebCore/workers',
      '../third_party/WebKit/WebCore/xml',
    ],
    'conditions': [
      ['OS=="linux"', {
        'non_feature_defines': [
          # Mozilla on Linux effectively uses uname -sm, but when running
          # 32-bit x86 code on an x86_64 processor, it uses
          # "Linux i686 (x86_64)".  Matching that would require making a
          # run-time determination.
          'WEBCORE_NAVIGATOR_PLATFORM="Linux i686"',
        ],
      }],
      ['OS=="mac"', {
        'non_feature_defines': [
          # Ensure that only Leopard features are used when doing the Mac build.
          'BUILDING_ON_LEOPARD',
          # Match Safari and Mozilla on Mac x86.
          'WEBCORE_NAVIGATOR_PLATFORM="MacIntel"',
        ],
        'webcore_include_dirs+': [
          # platform/graphics/cg and mac needs to come before
          # platform/graphics/chromium so that the Mac build picks up the
          # version of ImageBufferData.h in the cg directory and
          # FontPlatformData.h in the mac directory.  The + prepends this
          # directory to the list.
          # TODO(port): This shouldn't need to be prepended.
          # TODO(port): Eliminate dependency on platform/graphics/mac and
          # related directories.
          # platform/graphics/cg may need to stick around, though.
          '../third_party/WebKit/WebCore/platform/graphics/cg',
          '../third_party/WebKit/WebCore/platform/graphics/mac',
        ],
        'webcore_include_dirs': [
          # TODO(port): Eliminate dependency on platform/mac and related
          # directories.
          '../third_party/WebKit/WebCore/loader/archive/cf',
          '../third_party/WebKit/WebCore/platform/mac',
          '../third_party/WebKit/WebCore/platform/text/mac',
        ],
      }],
      ['OS=="win"', {
        'non_feature_defines': [
          'CRASH=__debugbreak',
          # Match Safari and Mozilla on Windows.
          'WEBCORE_NAVIGATOR_PLATFORM="Win32"',
        ],
        'webcore_include_dirs': [
          '../third_party/WebKit/WebCore/page/win',
          '../third_party/WebKit/WebCore/platform/graphics/win',
          '../third_party/WebKit/WebCore/platform/text/win',
          '../third_party/WebKit/WebCore/platform/win',
        ],
      }],
    ],
  },
  'includes': [
    '../build/common.gypi',
    '../third_party/WebKit/JavaScriptCore/JavaScriptCore.gypi',
    '../third_party/WebKit/WebCore/WebCore.gypi',
  ],
  'targets': [
    {
      # Currently, builders assume webkit.sln builds test_shell on windows.
      # We should change this, but for now allows trybot runs.
      # for now.
      'target_name': 'pull_in_test_shell',
      'type': 'none',
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'tools/test_shell/test_shell.gyp:*',
            'activex_shim_dll/activex_shim_dll.gyp:*',
          ],
        }],
      ],
    },
    {
      # This target creates config.h suitable for a WebKit-V8 build and
      # copies a few other files around as needed.
      'target_name': 'config',
      'type': 'none',
      'msvs_guid': '2E2D3301-2EC4-4C0F-B889-87073B30F673',
      'actions': [
        {
          'action_name': 'config.h',
          'inputs': [
            'config.h.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/config.h',
          ],
          # TODO(bradnelson): npapi.h, npruntime.h, npruntime_priv.h, and
          # stdint.h shouldn't be in the SHARED_INTERMEDIATE_DIR, it's very
          # global.
          'action': ['python', 'build/action_jsconfig.py', 'v8', '<(SHARED_INTERMEDIATE_DIR)/webkit', '<@(_inputs)'],

          'conditions': [
            ['OS=="win"', {
              'inputs': [
                '../third_party/WebKit/WebCore/bridge/npapi.h',
                '../third_party/WebKit/WebCore/bridge/npruntime.h',
                '../third_party/WebKit/WebCore/bindings/v8/npruntime_priv.h',
                '../third_party/WebKit/JavaScriptCore/os-win32/stdint.h',
              ],
            }],
          ],
        },
      ],
      'direct_dependent_settings': {
        'defines': [
          '<@(feature_defines)',
          '<@(non_feature_defines)',
        ],
        # Always prepend the directory containing config.h.  This is important,
        # because WebKit/JavaScriptCore has a config.h in it too.  The JSC
        # config.h shouldn't be used, and its directory winds up in
        # include_dirs in wtf and its dependents.  If the directory containing
        # the real config.h weren't prepended, other targets might wind up
        # picking up the wrong config.h, which can result in build failures or
        # even random runtime problems due to different components being built
        # with different configurations.
        #
        # The rightmost + is present because this direct_dependent_settings
        # section gets merged into the (nonexistent) target_defaults one,
        # eating the rightmost +.
        'include_dirs++': [
          '<(SHARED_INTERMEDIATE_DIR)/webkit',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': ['../build/win/system.gyp:cygwin'],
          'direct_dependent_settings': {
            'defines': [
              '__STD_C',
              '_CRT_SECURE_NO_DEPRECATE',
              '_SCL_SECURE_NO_DEPRECATE',
            ],
            'include_dirs': [
              '../third_party/WebKit/JavaScriptCore/os-win32',
              'build/JavaScriptCore',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'wtf',
      'type': '<(library)',
      'msvs_guid': 'AA8A5A85-592B-4357-BC60-E0E91E026AF6',
      'dependencies': [
        'config',
        '../third_party/icu38/icu38.gyp:icui18n',
        '../third_party/icu38/icu38.gyp:icuuc',
      ],
      'include_dirs': [
        '../third_party/WebKit/JavaScriptCore',
        '../third_party/WebKit/JavaScriptCore/wtf',
        '../third_party/WebKit/JavaScriptCore/wtf/unicode',
      ],
      'sources': [
        '<@(javascriptcore_files)',
        'build/precompiled_webkit.cc',
        'build/precompiled_webkit.h',
      ],
      'sources/': [
        # First exclude everything ...
        ['exclude', 'JavaScriptCore/'],
        # ... Then include what we want.
        ['include', 'JavaScriptCore/wtf/'],
        # GLib/GTK, even though its name doesn't really indicate.
        ['exclude', '/(GOwnPtr|glib/.*)\\.(cpp|h)$'],
        ['exclude', '(Default|Gtk|Mac|None|Qt|Win|Wx)\\.(cpp|mm)$'],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../third_party/WebKit/JavaScriptCore',
          '../third_party/WebKit/JavaScriptCore/wtf',
        ],
      },
      'export_dependent_settings': [
        'config',
        '../third_party/icu38/icu38.gyp:icui18n',
        '../third_party/icu38/icu38.gyp:icuuc',
      ],
      'configurations': {
        'Debug': {
          'msvs_precompiled_header': 'build/precompiled_webkit.h',
          'msvs_precompiled_source': 'build/precompiled_webkit.cc',
        },
      },
      'msvs_disabled_warnings': [4127, 4355, 4510, 4512, 4610, 4706],
      'conditions': [
        ['OS=="win"', {
          'sources/': [
            ['exclude', 'ThreadingPthreads\\.cpp$'],
            ['include', 'Thread(ing|Specific)Win\\.cpp$']
          ],
          'include_dirs': [
            'build',
            '../third_party/WebKit/JavaScriptCore/kjs',
            '../third_party/WebKit/JavaScriptCore/API',
            # These 3 do not seem to exist.
            '../third_party/WebKit/JavaScriptCore/bindings',
            '../third_party/WebKit/JavaScriptCore/bindings/c',
            '../third_party/WebKit/JavaScriptCore/bindings/jni',
            'pending',
            'pending/wtf',
          ],
          'include_dirs!': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit',
          ],
        }, { # OS != "win"
          'sources!': [
            'build/precompiled_webkit.cc',
            'build/precompiled_webkit.h',
           ],
        }],
        ['OS=="linux"', {
          'defines': ['WTF_USE_PTHREADS=1'],
          'direct_dependent_settings': {
            'defines': ['WTF_USE_PTHREADS=1'],
          },
        }],
      ],
    },
    {
      'target_name': 'pcre',
      'type': '<(library)',
      'dependencies': [
        'config',
        'wtf',
      ],
      'msvs_guid': '49909552-0B0C-4C14-8CF6-DB8A2ADE0934',
      'actions': [
        {
          'action_name': 'dftables',
          'inputs': [
            '../third_party/WebKit/JavaScriptCore/pcre/dftables',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/chartables.c',
          ],
          'action': ['perl', '-w', '<@(_inputs)', '<@(_outputs)'],
        },
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
      ],
      'sources': [
        '<@(javascriptcore_files)',
      ],
      'sources/': [
        # First exclude everything ...
        ['exclude', 'JavaScriptCore/'],
        # ... Then include what we want.
        ['include', 'JavaScriptCore/pcre/'],
        # ucptable.cpp is #included by pcre_ucp_searchfunchs.cpp and is not
        # intended to be compiled directly.
        ['exclude', 'JavaScriptCore/pcre/ucptable.cpp$'],
      ],
      'export_dependent_settings': [
        'wtf',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': ['../build/win/system.gyp:cygwin'],
        }],
      ],
    },
    {
      'target_name': 'webcore',
      'type': '<(library)',
      'msvs_guid': '1C16337B-ACF3-4D03-AA90-851C5B5EADA6',
      'dependencies': [
        'config',
        'pcre',
        'wtf',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../skia/skia.gyp:skia',
        '../third_party/libjpeg/libjpeg.gyp:libjpeg',
        '../third_party/libpng/libpng.gyp:libpng',
        '../third_party/libxml/libxml.gyp:libxml',
        '../third_party/libxslt/libxslt.gyp:libxslt',
        '../third_party/npapi/npapi.gyp:npapi',
        '../third_party/sqlite/sqlite.gyp:sqlite',
      ],
      'actions': [
        # Actions to build derived sources.
        {
          'action_name': 'CSSPropertyNames',
          'inputs': [
            '../third_party/WebKit/WebCore/css/makeprop.pl',
            '../third_party/WebKit/WebCore/css/CSSPropertyNames.in',
            '../third_party/WebKit/WebCore/css/SVGCSSPropertyNames.in',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/CSSPropertyNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSPropertyNames.h',
          ],
          'action': ['python', 'build/action_csspropertynames.py', '<@(_outputs)', '--', '<@(_inputs)'],
        },
        {
          'action_name': 'CSSValueKeywords',
          'inputs': [
            '../third_party/WebKit/WebCore/css/makevalues.pl',
            '../third_party/WebKit/WebCore/css/CSSValueKeywords.in',
            '../third_party/WebKit/WebCore/css/SVGCSSValueKeywords.in',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/CSSValueKeywords.c',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSValueKeywords.h',
          ],
          'action': ['python', 'build/action_cssvaluekeywords.py', '<@(_outputs)', '--', '<@(_inputs)'],
        },
        {
          'action_name': 'HTMLNames',
          'inputs': [
            '../third_party/WebKit/WebCore/dom/make_names.pl',
            '../third_party/WebKit/WebCore/html/HTMLTagNames.in',
            '../third_party/WebKit/WebCore/html/HTMLAttributeNames.in',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/HTMLNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/HTMLNames.h',
            '<(INTERMEDIATE_DIR)/HTMLElementFactory.cpp',
            # Pass --wrapperFactory to make_names to get these (JSC build?)
            #'<(INTERMEDIATE_DIR)/JSHTMLElementWrapperFactory.cpp',
            #'<(INTERMEDIATE_DIR)/JSHTMLElementWrapperFactory.h',
          ],
          'action': ['python', 'build/action_makenames.py', '<@(_outputs)', '--', '<@(_inputs)', '--', '--factory', '--extraDefines', '<(feature_defines)'],
          'process_outputs_as_sources': 1,
        },
        {
          'action_name': 'SVGNames',
          'inputs': [
            '../third_party/WebKit/WebCore/dom/make_names.pl',
            '../third_party/WebKit/WebCore/svg/svgtags.in',
            '../third_party/WebKit/WebCore/svg/svgattrs.in',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/SVGNames.cpp',
            '<(INTERMEDIATE_DIR)/SVGNames.h',
            '<(INTERMEDIATE_DIR)/SVGElementFactory.cpp',
            '<(INTERMEDIATE_DIR)/SVGElementFactory.h',
            # Pass --wrapperFactory to make_names to get these (JSC build?)
            #'<(INTERMEDIATE_DIR)/JSSVGElementWrapperFactory.cpp',
            #'<(INTERMEDIATE_DIR)/JSSVGElementWrapperFactory.h',
          ],
          'action': ['python', 'build/action_makenames.py', '<@(_outputs)', '--', '<@(_inputs)', '--', '--factory', '--extraDefines', '<(feature_defines)'],
          'process_outputs_as_sources': 1,
        },
        {
          'action_name': 'UserAgentStyleSheets',
          'inputs': [
            '../third_party/WebKit/WebCore/css/make-css-file-arrays.pl',
            '../third_party/WebKit/WebCore/css/html.css',
            '../third_party/WebKit/WebCore/css/quirks.css',
            '../third_party/WebKit/WebCore/css/view-source.css',
            '../third_party/WebKit/WebCore/css/themeChromiumLinux.css',
            '../third_party/WebKit/WebCore/css/themeWin.css',
            '../third_party/WebKit/WebCore/css/themeWinQuirks.css',
            '../third_party/WebKit/WebCore/css/svg.css',
            '../third_party/WebKit/WebCore/css/mediaControls.css',
            '../third_party/WebKit/WebCore/css/mediaControlsChromium.css',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/UserAgentStyleSheets.h',
            '<(INTERMEDIATE_DIR)/UserAgentStyleSheetsData.cpp',
          ],
          'action': ['python', 'build/action_useragentstylesheets.py', '<@(_outputs)', '--', '<@(_inputs)'],
          'process_outputs_as_sources': 1,
        },
        {
          'action_name': 'XLinkNames',
          'inputs': [
            '../third_party/WebKit/WebCore/dom/make_names.pl',
            '../third_party/WebKit/WebCore/svg/xlinkattrs.in',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/XLinkNames.cpp',
            '<(INTERMEDIATE_DIR)/XLinkNames.h',
          ],
          'action': ['python', 'build/action_makenames.py', '<@(_outputs)', '--', '<@(_inputs)', '--', '--extraDefines', '<(feature_defines)'],
          'process_outputs_as_sources': 1,
        },
        {
          'action_name': 'XMLNames',
          'inputs': [
            '../third_party/WebKit/WebCore/dom/make_names.pl',
            '../third_party/WebKit/WebCore/xml/xmlattrs.in',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/XMLNames.cpp',
            '<(INTERMEDIATE_DIR)/XMLNames.h',
          ],
          'action': ['python', 'build/action_makenames.py', '<@(_outputs)', '--', '<@(_inputs)', '--', '--extraDefines', '<(feature_defines)'],
          'process_outputs_as_sources': 1,
        },
        {
          'action_name': 'tokenizer',
          'inputs': [
            '../third_party/WebKit/WebCore/css/maketokenizer',
            '../third_party/WebKit/WebCore/css/tokenizer.flex',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/tokenizer.cpp',
          ],
          'action': ['python', 'build/action_maketokenizer.py', '<@(_outputs)', '--', '<@(_inputs)'],
        },
      ],
      'rules': [
        # Rules to build derived sources.
        {
          'rule_name': 'bison',
          'extension': 'y',
          'outputs': [
            '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).cpp',
            '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).h'
          ],
          'action': ['python', 'build/rule_bison.py', '<(RULE_INPUT_PATH)', '<(INTERMEDIATE_DIR)'],
          'process_outputs_as_sources': 1,
        },
        {
          'rule_name': 'gperf',
          'extension': 'gperf',
          # gperf output is only ever #included by other source files.  As
          # such, process_outputs_as_sources is off.  Some gperf output is
          # #included as *.c and some as *.cpp.  Since there's no way to tell
          # which one will be needed in a rule definition, declare both as
          # outputs.  The harness script will generate one file and copy it to
          # the other.
          #
          # This rule places outputs in SHARED_INTERMEDIATE_DIR because glue
          # needs access to HTMLEntityNames.c.
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/<(RULE_INPUT_ROOT).c',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/<(RULE_INPUT_ROOT).cpp',
          ],
          'action': ['python', 'build/rule_gperf.py', '<(RULE_INPUT_PATH)', '<(SHARED_INTERMEDIATE_DIR)/webkit'],
          'process_outputs_as_sources': 0,
        },
        # Rule to build generated JavaScript (V8) bindings from .idl source.
        {
          'rule_name': 'binding',
          'extension': 'idl',
          'msvs_external_rule': 1,
          'inputs': [
            '../third_party/WebKit/WebCore/bindings/scripts/generate-bindings.pl',
            '../third_party/WebKit/WebCore/bindings/scripts/CodeGenerator.pm',
            '../third_party/WebKit/WebCore/bindings/scripts/CodeGeneratorV8.pm',
            '../third_party/WebKit/WebCore/bindings/scripts/IDLParser.pm',
            '../third_party/WebKit/WebCore/bindings/scripts/IDLStructure.pm',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/bindings/V8<(RULE_INPUT_ROOT).cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8<(RULE_INPUT_ROOT).h',
          ],
          'variables': {
            'generator_include_dirs': [
              '--include', '../third_party/WebKit/WebCore/css',
              '--include', '../third_party/WebKit/WebCore/dom',
              '--include', '../third_party/WebKit/WebCore/html',
              '--include', '../third_party/WebKit/WebCore/page',
              '--include', '../third_party/WebKit/WebCore/plugins',
              '--include', '../third_party/WebKit/WebCore/svg',
              '--include', '../third_party/WebKit/WebCore/workers',
              '--include', '../third_party/WebKit/WebCore/xml',
            ],
          },
          'action': ['python', 'build/rule_binding.py', '<(RULE_INPUT_PATH)', '<(INTERMEDIATE_DIR)/bindings', '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings', '--', '<@(_inputs)', '--', '--defines', '<(feature_defines) LANGUAGE_JAVASCRIPT V8_BINDING', '--generator', 'V8', '<@(generator_include_dirs)'],
          # They are included by DerivedSourcesAllInOne.cpp instead.
          'process_outputs_as_sources': 0,
          'message': 'Generating binding from <(RULE_INPUT_PATH)',
        },
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings',
        '<@(webcore_include_dirs)',
      ],
      'sources': [
        # bison rule
        '../third_party/WebKit/WebCore/css/CSSGrammar.y',
        '../third_party/WebKit/WebCore/xml/XPathGrammar.y',

        # gperf rule
        '../third_party/WebKit/WebCore/html/DocTypeStrings.gperf',
        '../third_party/WebKit/WebCore/html/HTMLEntityNames.gperf',
        '../third_party/WebKit/WebCore/platform/ColorData.gperf',

        '<@(webcore_files)',

        # This file includes all the .cpp files generated from the above idl.
        '../third_party/WebKit/WebCore/bindings/v8/DerivedSourcesAllInOne.cpp',

        'extensions/v8/gc_extension.cc',
        'extensions/v8/gc_extension.h',
        'extensions/v8/gears_extension.cc',
        'extensions/v8/gears_extension.h',
        'extensions/v8/interval_extension.cc',
        'extensions/v8/interval_extension.h',
        'extensions/v8/playback_extension.cc',
        'extensions/v8/playback_extension.h',
        'extensions/v8/profiler_extension.cc',
        'extensions/v8/profiler_extension.h',
        'extensions/v8/benchmarking_extension.cc',
        'extensions/v8/benchmarking_extension.h',

        # For WebCoreSystemInterface, Mac-only.
        '../third_party/WebKit/WebKit/mac/WebCoreSupport/WebSystemInterface.m',
      ],
      'sources/': [
        # Exclude JSC custom bindings.
        ['exclude', '/third_party/WebKit/WebCore/bindings/js'],

        # SVG_FILTERS only.
        ['exclude', '/third_party/WebKit/WebCore/svg/SVG(FE|Filter)[^/]*\\.idl$'],

        # Fortunately, many things can be excluded by using broad patterns.

        # Exclude things that don't apply to the Chromium platform on the basis
        # of their enclosing directories and tags at the ends of their
        # filenames.
        ['exclude', '/(android|cairo|cf|cg|curl|gtk|linux|mac|opentype|posix|qt|soup|symbian|win|wx)/'],
        ['exclude', '(?<!Chromium)(SVGAllInOne|Android|Cairo|CF|CG|Curl|Gtk|Linux|Mac|OpenType|POSIX|Posix|Qt|Safari|Soup|Symbian|Win|Wx)\\.(cpp|mm?)$'],

        # JSC-only.
        ['exclude', '/third_party/WebKit/WebCore/inspector/JavaScript[^/]*\\.cpp$'],

        # ENABLE_OFFLINE_WEB_APPLICATIONS only.
        ['exclude', '/third_party/WebKit/WebCore/loader/appcache/'],

        # SVG_FILTERS only.
        ['exclude', '/third_party/WebKit/WebCore/(platform|svg)/graphics/filters/'],
        ['exclude', '/third_party/WebKit/WebCore/svg/Filter[^/]*\\.cpp$'],
        ['exclude', '/third_party/WebKit/WebCore/svg/SVG(FE|Filter)[^/]*\\.cpp$'],

        # Exclude PluginDebug.cpp since it doesn't compile properly without the
        # correct npapi.h inclusion (http://crbug.com/17127
        ['exclude', '/third_party/WebKit/WebCore/plugins/PluginDebug.cpp'],
        ['exclude', '/third_party/WebKit/WebCore/plugins/PluginDebug.h'],

        # Exclude some DB-related files.
        ['exclude', '/third_party/WebKit/WebCore/platform/sql/SQLiteFileSystem.cpp'],
      ],
      'sources!': [
        # Custom bindings in bindings/v8/custom exist for these.
        '../third_party/WebKit/WebCore/dom/EventListener.idl',
        '../third_party/WebKit/WebCore/dom/EventTarget.idl',
        '../third_party/WebKit/WebCore/html/VoidCallback.idl',

        # JSC-only.
        '../third_party/WebKit/WebCore/inspector/JavaScriptCallFrame.idl',

        # ENABLE_OFFLINE_WEB_APPLICATIONS only.
        '../third_party/WebKit/WebCore/loader/appcache/DOMApplicationCache.idl',

        # ENABLE_GEOLOCATION only.
        '../third_party/WebKit/WebCore/page/Geolocation.idl',
        '../third_party/WebKit/WebCore/page/Geoposition.idl',
        '../third_party/WebKit/WebCore/page/PositionCallback.idl',
        '../third_party/WebKit/WebCore/page/PositionError.idl',
        '../third_party/WebKit/WebCore/page/PositionErrorCallback.idl',

        # Bindings with custom Objective-C implementations.
        '../third_party/WebKit/WebCore/page/AbstractView.idl',

        # TODO(mark): I don't know why all of these are excluded.
        # Extra SVG bindings to exclude.
        '../third_party/WebKit/WebCore/svg/ElementTimeControl.idl',
        '../third_party/WebKit/WebCore/svg/SVGAnimatedPathData.idl',
        '../third_party/WebKit/WebCore/svg/SVGComponentTransferFunctionElement.idl',
        '../third_party/WebKit/WebCore/svg/SVGExternalResourcesRequired.idl',
        '../third_party/WebKit/WebCore/svg/SVGFitToViewBox.idl',
        '../third_party/WebKit/WebCore/svg/SVGHKernElement.idl',
        '../third_party/WebKit/WebCore/svg/SVGLangSpace.idl',
        '../third_party/WebKit/WebCore/svg/SVGLocatable.idl',
        '../third_party/WebKit/WebCore/svg/SVGStylable.idl',
        '../third_party/WebKit/WebCore/svg/SVGTests.idl',
        '../third_party/WebKit/WebCore/svg/SVGTransformable.idl',
        '../third_party/WebKit/WebCore/svg/SVGViewSpec.idl',
        '../third_party/WebKit/WebCore/svg/SVGZoomAndPan.idl',

        # TODO(mark): I don't know why these are excluded, either.
        # Someone (me?) should figure it out and add appropriate comments.
        '../third_party/WebKit/WebCore/css/CSSUnknownRule.idl',

        # A few things can't be excluded by patterns.  List them individually.
        
        # Don't build StorageNamespace.  We have our own implementation.
        '../third_party/WebKit/WebCore/storage/StorageNamespace.cpp',
        
        # Use history/BackForwardListChromium.cpp instead.
        '../third_party/WebKit/WebCore/history/BackForwardList.cpp',

        # Use loader/icon/IconDatabaseNone.cpp instead.
        '../third_party/WebKit/WebCore/loader/icon/IconDatabase.cpp',

        # Use platform/KURLGoogle.cpp instead.
        '../third_party/WebKit/WebCore/platform/KURL.cpp',

        # Use platform/MIMETypeRegistryChromium.cpp instead.
        '../third_party/WebKit/WebCore/platform/MIMETypeRegistry.cpp',

        # USE_NEW_THEME only.
        '../third_party/WebKit/WebCore/platform/Theme.cpp',

        # Exclude some, but not all, of plugins.
        '../third_party/WebKit/WebCore/plugins/PluginDatabase.cpp',
        '../third_party/WebKit/WebCore/plugins/PluginInfoStore.cpp',
        '../third_party/WebKit/WebCore/plugins/PluginMainThreadScheduler.cpp',
        '../third_party/WebKit/WebCore/plugins/PluginPackage.cpp',
        '../third_party/WebKit/WebCore/plugins/PluginStream.cpp',
        '../third_party/WebKit/WebCore/plugins/PluginView.cpp',
        '../third_party/WebKit/WebCore/plugins/npapi.cpp',

        # Use LinkHashChromium.cpp instead
        '../third_party/WebKit/WebCore/platform/LinkHash.cpp',

        # Don't build these.
        # TODO(mark): I don't know exactly why these are excluded.  It would
        # be nice to provide more explicit comments.  Some of these do actually
        # compile.
        '../third_party/WebKit/WebCore/dom/StaticStringList.cpp',
        '../third_party/WebKit/WebCore/loader/icon/IconFetcher.cpp',
        '../third_party/WebKit/WebCore/loader/UserStyleSheetLoader.cpp',
        '../third_party/WebKit/WebCore/platform/graphics/GraphicsLayer.cpp',
        '../third_party/WebKit/WebCore/platform/graphics/RenderLayerBacking.cpp',
        '../third_party/WebKit/WebCore/platform/graphics/RenderLayerCompositor.cpp',

      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/webkit',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings',
          '<@(webcore_include_dirs)',
        ],
        'mac_framework_dirs': [
          '$(SDKROOT)/System/Library/Frameworks/ApplicationServices.framework/Frameworks',
        ],
      },
      'export_dependent_settings': [
        'wtf',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../skia/skia.gyp:skia',
        '../third_party/npapi/npapi.gyp:npapi',
      ],
      'link_settings': {
        'mac_bundle_resources': [
          '../third_party/WebKit/WebCore/Resources/aliasCursor.png',
          '../third_party/WebKit/WebCore/Resources/cellCursor.png',
          '../third_party/WebKit/WebCore/Resources/contextMenuCursor.png',
          '../third_party/WebKit/WebCore/Resources/copyCursor.png',
          '../third_party/WebKit/WebCore/Resources/crossHairCursor.png',
          '../third_party/WebKit/WebCore/Resources/eastResizeCursor.png',
          '../third_party/WebKit/WebCore/Resources/eastWestResizeCursor.png',
          '../third_party/WebKit/WebCore/Resources/helpCursor.png',
          '../third_party/WebKit/WebCore/Resources/linkCursor.png',
          '../third_party/WebKit/WebCore/Resources/missingImage.png',
          '../third_party/WebKit/WebCore/Resources/moveCursor.png',
          '../third_party/WebKit/WebCore/Resources/noDropCursor.png',
          '../third_party/WebKit/WebCore/Resources/noneCursor.png',
          '../third_party/WebKit/WebCore/Resources/northEastResizeCursor.png',
          '../third_party/WebKit/WebCore/Resources/northEastSouthWestResizeCursor.png',
          '../third_party/WebKit/WebCore/Resources/northResizeCursor.png',
          '../third_party/WebKit/WebCore/Resources/northSouthResizeCursor.png',
          '../third_party/WebKit/WebCore/Resources/northWestResizeCursor.png',
          '../third_party/WebKit/WebCore/Resources/northWestSouthEastResizeCursor.png',
          '../third_party/WebKit/WebCore/Resources/notAllowedCursor.png',
          '../third_party/WebKit/WebCore/Resources/progressCursor.png',
          '../third_party/WebKit/WebCore/Resources/southEastResizeCursor.png',
          '../third_party/WebKit/WebCore/Resources/southResizeCursor.png',
          '../third_party/WebKit/WebCore/Resources/southWestResizeCursor.png',
          '../third_party/WebKit/WebCore/Resources/verticalTextCursor.png',
          '../third_party/WebKit/WebCore/Resources/waitCursor.png',
          '../third_party/WebKit/WebCore/Resources/westResizeCursor.png',
          '../third_party/WebKit/WebCore/Resources/zoomInCursor.png',
          '../third_party/WebKit/WebCore/Resources/zoomOutCursor.png',
        ],
      },
      'hard_dependency': 1,
      'mac_framework_dirs': [
        '$(SDKROOT)/System/Library/Frameworks/ApplicationServices.framework/Frameworks',
      ],
      'msvs_disabled_warnings': [
        4138, 4244, 4291, 4305, 4344, 4355, 4521, 4099,
      ],
      'scons_line_length' : 1,
      'xcode_settings': {
        # Some Mac-specific parts of WebKit won't compile without having this
        # prefix header injected.
        # TODO(mark): make this a first-class setting.
        'GCC_PREFIX_HEADER': '../third_party/WebKit/WebCore/WebCorePrefix.h',
      },
      'conditions': [
        ['javascript_engine=="v8"', {
          'dependencies': [
            '../v8/tools/gyp/v8.gyp:v8',
          ],
          'export_dependent_settings': [
            '../v8/tools/gyp/v8.gyp:v8',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:fontconfig',
            '../build/linux/system.gyp:gtk',
          ],
          'sources': [
            '../third_party/WebKit/WebCore/platform/graphics/chromium/VDMXParser.cpp',
            '../third_party/WebKit/WebCore/platform/graphics/chromium/HarfbuzzSkia.cpp',
          ],
          'sources!': [
            # Not yet ported to Linux.
            '../third_party/WebKit/WebCore/platform/graphics/chromium/FontCustomPlatformData.cpp',
          ],
          'sources/': [
            # Cherry-pick files excluded by the broader regular expressions above.
            ['include', 'third_party/WebKit/WebCore/platform/chromium/KeyCodeConversionGtk\\.cpp$'],
            ['include', 'third_party/WebKit/WebCore/platform/graphics/chromium/FontCacheLinux\\.cpp$'],
            ['include', 'third_party/WebKit/WebCore/platform/graphics/chromium/FontLinux\\.cpp$'],
            ['include', 'third_party/WebKit/WebCore/platform/graphics/chromium/FontPlatformDataLinux\\.cpp$'],
            ['include', 'third_party/WebKit/WebCore/platform/graphics/chromium/GlyphPageTreeNodeLinux\\.cpp$'],
            ['include', 'third_party/WebKit/WebCore/platform/graphics/chromium/SimpleFontDataLinux\\.cpp$'],
          ],
          'cflags': [
            # -Wno-multichar for:
            #   .../WebCore/platform/image-decoders/bmp/BMPImageDecoder.cpp
            '-Wno-multichar',
            # WebCore does not work with strict aliasing enabled.
            # https://bugs.webkit.org/show_bug.cgi?id=25864
            '-fno-strict-aliasing',
          ],
        }],
        ['OS=="mac"', {
          'actions': [
            {
              # Allow framework-style #include of
              # <WebCore/WebCoreSystemInterface.h>.
              'action_name': 'WebCoreSystemInterface.h',
              'inputs': [
                '../third_party/WebKit/WebCore/platform/mac/WebCoreSystemInterface.h',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/WebCore/WebCoreSystemInterface.h',
              ],
              'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
            },
          ],
          'include_dirs': [
            '../third_party/WebKit/WebKitLibraries',
          ],
          'sources/': [
            # Additional files from the WebCore Mac build that are presently
            # used in the WebCore Chromium Mac build too.

            # The Mac build is PLATFORM_CF but does not use CFNetwork.
            ['include', 'CF\\.cpp$'],
            ['exclude', '/network/cf/'],

            # The Mac build is PLATFORM_CG too.  platform/graphics/cg is the
            # only place that CG files we want to build are located, and not
            # all of them even have a CG suffix, so just add them by a
            # regexp matching their directory.
            ['include', '/platform/graphics/cg/[^/]*(?<!Win)?\\.(cpp|mm?)$'],

            # Use native Mac font code from WebCore.
            ['include', '/platform/(graphics/)?mac/[^/]*Font[^/]*\\.(cpp|mm?)$'],

            # Cherry-pick some files that can't be included by broader regexps.
            # Some of these are used instead of Chromium platform files, see
            # the specific exclusions in the "sources!" list below.
            ['include', '/third_party/WebKit/WebCore/loader/archive/cf/LegacyWebArchive\\.cpp$'],
            ['include', '/third_party/WebKit/WebCore/platform/graphics/mac/ColorMac\\.mm$'],
            ['include', '/third_party/WebKit/WebCore/platform/graphics/mac/GlyphPageTreeNodeMac\\.cpp$'],
            ['include', '/third_party/WebKit/WebCore/platform/graphics/mac/GraphicsContextMac\\.mm$'],
            ['include', '/third_party/WebKit/WebCore/platform/mac/BlockExceptions\\.mm$'],
            ['include', '/third_party/WebKit/WebCore/platform/mac/LocalCurrentGraphicsContext\\.mm$'],
            ['include', '/third_party/WebKit/WebCore/platform/mac/PurgeableBufferMac\\.cpp$'],
            ['include', '/third_party/WebKit/WebCore/platform/mac/ScrollbarThemeMac\\.mm$'],
            ['include', '/third_party/WebKit/WebCore/platform/mac/WebCoreSystemInterface\\.mm$'],
            ['include', '/third_party/WebKit/WebCore/platform/mac/WebCoreTextRenderer\\.mm$'],
            ['include', '/third_party/WebKit/WebCore/platform/text/mac/ShapeArabic\\.c$'],
            ['include', '/third_party/WebKit/WebCore/platform/text/mac/String(Impl)?Mac\\.mm$'],

            ['include', '/third_party/WebKit/WebKit/mac/WebCoreSupport/WebSystemInterface\\.m$'],
          ],
          'sources!': [
            # The Mac currently uses FontCustomPlatformData.cpp from
            # platform/graphics/mac, included by regex above, instead.
            '../third_party/WebKit/WebCore/platform/graphics/chromium/FontCustomPlatformData.cpp',

            # The Mac currently uses ScrollbarThemeMac.mm, included by regex
            # above, instead of ScrollbarThemeChromium.cpp.
            '../third_party/WebKit/WebCore/platform/chromium/ScrollbarThemeChromium.cpp',

            # These Skia files aren't currently built on the Mac, which uses
            # CoreGraphics directly for this portion of graphics handling.
            '../third_party/WebKit/WebCore/platform/graphics/skia/FloatPointSkia.cpp',
            '../third_party/WebKit/WebCore/platform/graphics/skia/FloatRectSkia.cpp',
            '../third_party/WebKit/WebCore/platform/graphics/skia/GradientSkia.cpp',
            '../third_party/WebKit/WebCore/platform/graphics/skia/GraphicsContextSkia.cpp',
            '../third_party/WebKit/WebCore/platform/graphics/skia/ImageBufferSkia.cpp',
            '../third_party/WebKit/WebCore/platform/graphics/skia/ImageSkia.cpp',
            '../third_party/WebKit/WebCore/platform/graphics/skia/ImageSourceSkia.cpp',
            '../third_party/WebKit/WebCore/platform/graphics/skia/IntPointSkia.cpp',
            '../third_party/WebKit/WebCore/platform/graphics/skia/IntRectSkia.cpp',
            '../third_party/WebKit/WebCore/platform/graphics/skia/PathSkia.cpp',
            '../third_party/WebKit/WebCore/platform/graphics/skia/PatternSkia.cpp',
            '../third_party/WebKit/WebCore/platform/graphics/skia/TransformationMatrixSkia.cpp',

            # RenderThemeChromiumSkia is not used on mac since RenderThemeChromiumMac
            # does not reference the Skia code that is used by Windows and Linux.
            '../third_party/WebKit/WebCore/rendering/RenderThemeChromiumSkia.cpp',

            # Skia image-decoders are also not used on mac.  CoreGraphics
            # is used directly instead.
            '../third_party/WebKit/WebCore/platform/image-decoders/ImageDecoder.h',
            '../third_party/WebKit/WebCore/platform/image-decoders/bmp/BMPImageDecoder.cpp',
            '../third_party/WebKit/WebCore/platform/image-decoders/bmp/BMPImageDecoder.h',
            '../third_party/WebKit/WebCore/platform/image-decoders/bmp/BMPImageReader.cpp',
            '../third_party/WebKit/WebCore/platform/image-decoders/bmp/BMPImageReader.h',
            '../third_party/WebKit/WebCore/platform/image-decoders/gif/GIFImageDecoder.cpp',
            '../third_party/WebKit/WebCore/platform/image-decoders/gif/GIFImageDecoder.h',
            '../third_party/WebKit/WebCore/platform/image-decoders/gif/GIFImageReader.cpp',
            '../third_party/WebKit/WebCore/platform/image-decoders/gif/GIFImageReader.h',
            '../third_party/WebKit/WebCore/platform/image-decoders/ico/ICOImageDecoder.cpp',
            '../third_party/WebKit/WebCore/platform/image-decoders/ico/ICOImageDecoder.h',
            '../third_party/WebKit/WebCore/platform/image-decoders/jpeg/JPEGImageDecoder.cpp',
            '../third_party/WebKit/WebCore/platform/image-decoders/jpeg/JPEGImageDecoder.h',
            '../third_party/WebKit/WebCore/platform/image-decoders/png/PNGImageDecoder.cpp',
            '../third_party/WebKit/WebCore/platform/image-decoders/png/PNGImageDecoder.h',
            '../third_party/WebKit/WebCore/platform/image-decoders/skia/ImageDecoderSkia.cpp',
            '../third_party/WebKit/WebCore/platform/image-decoders/xbm/XBMImageDecoder.cpp',
            '../third_party/WebKit/WebCore/platform/image-decoders/xbm/XBMImageDecoder.h',
          ],
          'link_settings': {
            'libraries': [
              '../third_party/WebKit/WebKitLibraries/libWebKitSystemInterfaceLeopard.a',
            ],
          },
          'direct_dependent_settings': {
            'include_dirs': [
              '../third_party/WebKit/WebKitLibraries',
              '../third_party/WebKit/WebKit/mac/WebCoreSupport',
            ],
          },
        }],
        ['OS=="win"', {
          'dependencies': ['../build/win/system.gyp:cygwin'],
          'sources/': [
            ['exclude', 'Posix\\.cpp$'],
            ['include', '/opentype/'],
            ['include', '/TransparencyWin\\.cpp$'],
            ['include', '/SkiaFontWin\\.cpp$'],
          ],
          'defines': [
            '__PRETTY_FUNCTION__=__FUNCTION__',
            'DISABLE_ACTIVEX_TYPE_CONVERSION_MPLAYER2',
          ],
          # This is needed because Event.h in this directory is blocked
          # by a system header on windows.
          'include_dirs++': ['../third_party/WebKit/WebCore/dom'],
          'direct_dependent_settings': {
            'include_dirs+++': ['../third_party/WebKit/WebCore/dom'],
          },
        }],
        ['OS!="linux"', {'sources/': [['exclude', '(Gtk|Linux)\\.cpp$']]}],
        ['OS!="mac"', {'sources/': [['exclude', 'Mac\\.(cpp|mm?)$']]}],
        ['OS!="win"', {
          'sources/': [
            ['exclude', 'Win\\.cpp$'],
            ['exclude', '/(Windows|Uniscribe)[^/]*\\.cpp$']
          ],
        }],
      ],
    },
    {
      'target_name': 'webkit',
      'type': '<(library)',
      'msvs_guid': '5ECEC9E5-8F23-47B6-93E0-C3B328B3BE65',
      'dependencies': [
        'webcore',
      ],
      'include_dirs': [
        'api/public',
        'api/src',
      ],
      'defines': [
        'WEBKIT_IMPLEMENTATION',
      ],
      'sources': [
        'api/public/gtk/WebInputEventFactory.h',
        'api/public/x11/WebScreenInfoFactory.h',
        'api/public/mac/WebInputEventFactory.h',
        'api/public/mac/WebScreenInfoFactory.h',
        'api/public/WebCache.h',
        'api/public/WebCanvas.h',
        'api/public/WebClipboard.h',
        'api/public/WebColor.h',
        'api/public/WebCommon.h',
        'api/public/WebCompositionCommand.h',
        'api/public/WebConsoleMessage.h',
        'api/public/WebCString.h',
        'api/public/WebCursorInfo.h',
        'api/public/WebData.h',
        'api/public/WebDataSource.h',
        'api/public/WebDragData.h',
        'api/public/WebFindOptions.h',
        'api/public/WebForm.h',
        'api/public/WebHistoryItem.h',
        'api/public/WebHTTPBody.h',
        'api/public/WebImage.h',
        'api/public/WebInputEvent.h',
        'api/public/WebKit.h',
        'api/public/WebKitClient.h',
        'api/public/WebLocalizedString.h',
        'api/public/WebMediaPlayer.h',
        'api/public/WebMediaPlayerClient.h',
        'api/public/WebMimeRegistry.h',
        'api/public/WebNavigationType.h',
        'api/public/WebNode.h',
        'api/public/WebNonCopyable.h',
        'api/public/WebPluginListBuilder.h',
        'api/public/WebPoint.h',
        'api/public/WebPopupMenu.h',
        'api/public/WebPopupMenuInfo.h',
        'api/public/WebRect.h',
        'api/public/WebScreenInfo.h',
        'api/public/WebScriptSource.h',
        'api/public/WebSize.h',
        'api/public/WebStorageArea.h',
        'api/public/WebStorageNamespace.h',
        'api/public/WebString.h',
        'api/public/WebTextDirection.h',
        'api/public/WebURL.h',
        'api/public/WebURLError.h',
        'api/public/WebURLLoader.h',
        'api/public/WebURLLoaderClient.h',
        'api/public/WebURLRequest.h',
        'api/public/WebURLResponse.h',
        'api/public/WebVector.h',
        'api/public/WebWidget.h',
        'api/public/WebWidgetClient.h',
        'api/public/win/WebInputEventFactory.h',
        'api/public/win/WebSandboxSupport.h',
        'api/public/win/WebScreenInfoFactory.h',
        'api/public/win/WebScreenInfoFactory.h',
        'api/src/ChromiumBridge.cpp',
        'api/src/ChromiumCurrentTime.cpp',
        'api/src/ChromiumThreading.cpp',
        'api/src/gtk/WebFontInfo.cpp',
        'api/src/gtk/WebFontInfo.h',
        'api/src/gtk/WebInputEventFactory.cpp',
        'api/src/x11/WebScreenInfoFactory.cpp',
        'api/src/mac/WebInputEventFactory.mm',
        'api/src/mac/WebScreenInfoFactory.mm',
        'api/src/LocalizedStrings.cpp',
        'api/src/MediaPlayerPrivateChromium.cpp',
        'api/src/ResourceHandle.cpp',
        'api/src/StorageNamespaceProxy.cpp',
        'api/src/StorageNamespaceProxy.h',
        'api/src/TemporaryGlue.h',
        'api/src/WebCache.cpp',
        'api/src/WebCString.cpp',
        'api/src/WebCursorInfo.cpp',
        'api/src/WebData.cpp',
        'api/src/WebDragData.cpp',
        'api/src/WebForm.cpp',
        'api/src/WebHistoryItem.cpp',
        'api/src/WebHTTPBody.cpp',
        'api/src/WebImageCG.cpp',
        'api/src/WebImageSkia.cpp',
        'api/src/WebInputEvent.cpp',
        'api/src/WebKit.cpp',
        'api/src/WebMediaPlayerClientImpl.cpp',
        'api/src/WebMediaPlayerClientImpl.h',
        'api/src/WebNode.cpp',
        'api/src/WebPluginListBuilderImpl.cpp',
        'api/src/WebPluginListBuilderImpl.h',
        'api/src/WebStorageAreaImpl.cpp',
        'api/src/WebStorageAreaImpl.h',
        'api/src/WebStorageNamespaceImpl.cpp',
        'api/src/WebStorageNamespaceImpl.h',
        'api/src/WebString.cpp',
        'api/src/WebURL.cpp',
        'api/src/WebURLRequest.cpp',
        'api/src/WebURLRequestPrivate.h',
        'api/src/WebURLResponse.cpp',
        'api/src/WebURLResponsePrivate.h',
        'api/src/WebURLError.cpp',
        'api/src/WrappedResourceRequest.h',
        'api/src/WrappedResourceResponse.h',
        'api/src/win/WebInputEventFactory.cpp',
        'api/src/win/WebScreenInfoFactory.cpp',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:x11',
            '../build/linux/system.gyp:gtk',
          ],
          'include_dirs': [
            'api/public/x11',
            'api/public/gtk',
            'api/public/linux',
          ],
        }, { # else: OS!="linux"
          'sources/': [
            ['exclude', '/gtk/'],
            ['exclude', '/x11/'],
          ],
        }],
        ['OS=="mac"', {
          'include_dirs': [
            'api/public/mac',
          ],
          'sources/': [
            ['exclude', 'Skia\\.cpp$'],
          ],
        }, { # else: OS!="mac"
          'sources/': [
            ['exclude', '/mac/'],
            ['exclude', 'CG\\.cpp$'],
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            'api/public/win',
          ],
        }, { # else: OS!="win"
          'sources/': [['exclude', '/win/']],
        }],
      ],
    },
    {
      'target_name': 'webkit_resources',
      'type': 'none',
      'msvs_guid': '0B469837-3D46-484A-AFB3-C5A6C68730B9',
      'variables': {
        'grit_path': '../tools/grit/grit.py',
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/webkit',
      },
      'actions': [
        {
          'action_name': 'webkit_resources',
          'variables': {
            'input_path': 'glue/webkit_resources.grd',
          },
          'inputs': [
            '<(input_path)',
          ],
          'outputs': [
            '<(grit_out_dir)/grit/webkit_resources.h',
            '<(grit_out_dir)/webkit_resources.pak',
            '<(grit_out_dir)/webkit_resources.rc',
          ],
          'action': ['python', '<(grit_path)', '-i', '<(input_path)', 'build', '-o', '<(grit_out_dir)'],
          'message': 'Generating resources from <(input_path)',
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/webkit',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': ['../build/win/system.gyp:cygwin'],
        }],
      ],
    },
    {
      'target_name': 'webkit_strings',
      'type': 'none',
      'msvs_guid': '60B43839-95E6-4526-A661-209F16335E0E',
      'variables': {
        'grit_path': '../tools/grit/grit.py',
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/webkit',
      },
      'actions': [
        {
          'action_name': 'webkit_strings',
          'variables': {
            'input_path': 'glue/webkit_strings.grd',
          },
          'inputs': [
            '<(input_path)',
          ],
          'outputs': [
            '<(grit_out_dir)/grit/webkit_strings.h',
            '<(grit_out_dir)/webkit_strings_da.pak',
            '<(grit_out_dir)/webkit_strings_da.rc',
            '<(grit_out_dir)/webkit_strings_en-US.pak',
            '<(grit_out_dir)/webkit_strings_en-US.rc',
            '<(grit_out_dir)/webkit_strings_he.pak',
            '<(grit_out_dir)/webkit_strings_he.rc',
            '<(grit_out_dir)/webkit_strings_zh-TW.pak',
            '<(grit_out_dir)/webkit_strings_zh-TW.rc',
          ],
          'action': ['python', '<(grit_path)', '-i', '<(input_path)', 'build', '-o', '<(grit_out_dir)'],
          'message': 'Generating resources from <(input_path)',
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/webkit',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': ['../build/win/system.gyp:cygwin'],
        }],
      ],
    },
    {
      'target_name': 'glue',
      'type': '<(library)',
      'msvs_guid': 'C66B126D-0ECE-4CA2-B6DC-FA780AFBBF09',
      'dependencies': [
        '../net/net.gyp:net',
        'inspector_resources',
        'webcore',
        'webkit',
        'webkit_resources',
        'webkit_strings',
      ],
      'actions': [
        {
          'action_name': 'webkit_version',
          'inputs': [
            'build/webkit_version.py',
            '../third_party/WebKit/WebCore/Configurations/Version.xcconfig',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/webkit_version.h',
          ],
          'action': ['python', '<@(_inputs)', '<(INTERMEDIATE_DIR)'],
        },
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
      ],
      'sources': [
        # This list contains all .h, .cc, and .mm files in glue except for
        # those in the test subdirectory and those with unittest in in their
        # names.
        'glue/devtools/devtools_rpc.cc',
        'glue/devtools/devtools_rpc.h',
        'glue/devtools/devtools_rpc_js.h',
        'glue/devtools/bound_object.cc',
        'glue/devtools/bound_object.h',
        'glue/devtools/debugger_agent.h',
        'glue/devtools/debugger_agent_impl.cc',
        'glue/devtools/debugger_agent_impl.h',
        'glue/devtools/debugger_agent_manager.cc',
        'glue/devtools/debugger_agent_manager.h',
        'glue/devtools/dom_agent.h',
        'glue/devtools/dom_agent_impl.cc',
        'glue/devtools/dom_agent_impl.h',
        'glue/devtools/tools_agent.h',
        'glue/media/buffered_data_source.cc',
        'glue/media/buffered_data_source.h',
        'glue/media/media_resource_loader_bridge_factory.cc',
        'glue/media/media_resource_loader_bridge_factory.h',
        'glue/media/simple_data_source.cc',
        'glue/media/simple_data_source.h',
        'glue/media/video_renderer_impl.cc',
        'glue/media/video_renderer_impl.h',
        'glue/plugins/mozilla_extensions.cc',
        'glue/plugins/mozilla_extensions.h',
        'glue/plugins/nphostapi.h',
        'glue/plugins/gtk_plugin_container.h',
        'glue/plugins/gtk_plugin_container.cc',
        'glue/plugins/gtk_plugin_container_manager.h',
        'glue/plugins/gtk_plugin_container_manager.cc',
        'glue/plugins/plugin_constants_win.h',
        'glue/plugins/plugin_host.cc',
        'glue/plugins/plugin_host.h',
        'glue/plugins/plugin_instance.cc',
        'glue/plugins/plugin_instance.h',
        'glue/plugins/plugin_lib.cc',
        'glue/plugins/plugin_lib.h',
        'glue/plugins/plugin_lib_linux.cc',
        'glue/plugins/plugin_lib_mac.mm',
        'glue/plugins/plugin_lib_win.cc',
        'glue/plugins/plugin_list.cc',
        'glue/plugins/plugin_list.h',
        'glue/plugins/plugin_list_linux.cc',
        'glue/plugins/plugin_list_mac.mm',
        'glue/plugins/plugin_list_win.cc',
        'glue/plugins/plugin_stream.cc',
        'glue/plugins/plugin_stream.h',
        'glue/plugins/plugin_stream_posix.cc',
        'glue/plugins/plugin_stream_url.cc',
        'glue/plugins/plugin_stream_url.h',
        'glue/plugins/plugin_stream_win.cc',
        'glue/plugins/plugin_string_stream.cc',
        'glue/plugins/plugin_string_stream.h',
        'glue/plugins/plugin_stubs.cc',
        'glue/plugins/webplugin_delegate_impl.cc',
        'glue/plugins/webplugin_delegate_impl.h',
        'glue/plugins/webplugin_delegate_impl_gtk.cc',
        'glue/plugins/webplugin_delegate_impl_mac.mm',
        'glue/alt_error_page_resource_fetcher.cc',
        'glue/alt_error_page_resource_fetcher.h',
        'glue/autofill_form.cc',
        'glue/autofill_form.h',
        'glue/back_forward_list_client_impl.cc',
        'glue/back_forward_list_client_impl.h',
        'glue/chrome_client_impl.cc',
        'glue/chrome_client_impl.h',
        'glue/chromium_bridge_impl.cc',
        'glue/context_menu.h',
        'glue/context_menu_client_impl.cc',
        'glue/context_menu_client_impl.h',
        'glue/cpp_binding_example.cc',
        'glue/cpp_binding_example.h',
        'glue/cpp_bound_class.cc',
        'glue/cpp_bound_class.h',
        'glue/cpp_variant.cc',
        'glue/cpp_variant.h',
        'glue/dom_operations.cc',
        'glue/dom_operations.h',
        'glue/dom_operations_private.h',
        'glue/dom_serializer.cc',
        'glue/dom_serializer.h',
        'glue/dom_serializer_delegate.h',
        'glue/dragclient_impl.cc',
        'glue/dragclient_impl.h',
        'glue/editor_client_impl.cc',
        'glue/editor_client_impl.h',
        'glue/entity_map.cc',
        'glue/entity_map.h',
        'glue/event_conversion.cc',
        'glue/event_conversion.h',
        'glue/feed_preview.cc',
        'glue/feed_preview.h',
        'glue/form_data.h',
        'glue/glue_accessibility_object.cc',
        'glue/glue_accessibility_object.h',
        'glue/glue_serialize.cc',
        'glue/glue_serialize.h',
        'glue/glue_util.cc',
        'glue/glue_util.h',
        'glue/image_decoder.cc',
        'glue/image_decoder.h',
        'glue/image_resource_fetcher.cc',
        'glue/image_resource_fetcher.h',
        'glue/inspector_client_impl.cc',
        'glue/inspector_client_impl.h',
        'glue/multipart_response_delegate.cc',
        'glue/multipart_response_delegate.h',
        'glue/npruntime_util.cc',
        'glue/npruntime_util.h',
        'glue/password_autocomplete_listener.cc',
        'glue/password_autocomplete_listener.h',
        'glue/password_form.h',
        'glue/password_form_dom_manager.cc',
        'glue/password_form_dom_manager.h',
        'glue/resource_fetcher.cc',
        'glue/resource_fetcher.h',
        'glue/resource_loader_bridge.cc',
        'glue/resource_loader_bridge.h',
        'glue/resource_type.h',
        'glue/scoped_clipboard_writer_glue.h',
        'glue/searchable_form_data.cc',
        'glue/searchable_form_data.h',
        'glue/simple_webmimeregistry_impl.cc',
        'glue/simple_webmimeregistry_impl.h',
        'glue/stacking_order_iterator.cc',
        'glue/stacking_order_iterator.h',
        'glue/temporary_glue.cc',
        'glue/webaccessibility.h',
        'glue/webaccessibilitymanager.h',
        'glue/webaccessibilitymanager_impl.cc',
        'glue/webaccessibilitymanager_impl.h',
        'glue/webappcachecontext.cc',
        'glue/webappcachecontext.h',
        'glue/webclipboard_impl.cc',
        'glue/webclipboard_impl.h',
        'glue/webcursor.cc',
        'glue/webcursor.h',
        'glue/webcursor_gtk.cc',
        'glue/webcursor_gtk_data.h',
        'glue/webcursor_mac.mm',
        'glue/webcursor_win.cc',
        'glue/webdatasource_impl.cc',
        'glue/webdatasource_impl.h',
        'glue/webdevtoolsagent.h',
        'glue/webdevtoolsagent_delegate.h',
        'glue/webdevtoolsagent_impl.cc',
        'glue/webdevtoolsagent_impl.h',
        'glue/webdevtoolsclient.h',
        'glue/webdevtoolsclient_delegate.h',
        'glue/webdevtoolsclient_impl.cc',
        'glue/webdevtoolsclient_impl.h',
        'glue/webdropdata.cc',
        'glue/webdropdata_win.cc',
        'glue/webdropdata.h',
        'glue/webframe.h',
        'glue/webframe_impl.cc',
        'glue/webframe_impl.h',
        'glue/webframeloaderclient_impl.cc',
        'glue/webframeloaderclient_impl.h',
        'glue/webkit_glue.cc',
        'glue/webkit_glue.h',
        'glue/webkitclient_impl.cc',
        'glue/webkitclient_impl.h',
        'glue/webmediaplayer_impl.h',
        'glue/webmediaplayer_impl.cc',
        'glue/webmenuitem.h',
        'glue/webmenurunner_mac.h',
        'glue/webmenurunner_mac.mm',
        'glue/webplugin.h',
        'glue/webplugin_delegate.cc',
        'glue/webplugin_delegate.h',
        'glue/webplugin_impl.cc',
        'glue/webplugin_impl.h',
        'glue/webplugininfo.h',
        'glue/webpopupmenu_impl.cc',
        'glue/webpopupmenu_impl.h',
        'glue/webpreferences.h',
        'glue/webtextinput.h',
        'glue/webtextinput_impl.cc',
        'glue/webtextinput_impl.h',
        'glue/webthemeengine_impl_win.cc',
        'glue/weburlloader_impl.cc',
        'glue/weburlloader_impl.h',
        'glue/webview.h',
        'glue/webview_delegate.cc',
        'glue/webview_delegate.h',
        'glue/webview_impl.cc',
        'glue/webview_impl.h',
        'glue/webworker_impl.cc',
        'glue/webworker_impl.h',
        'glue/webworkerclient_impl.cc',
        'glue/webworkerclient_impl.h',
        'glue/window_open_disposition.h',
        'glue/window_open_disposition.cc',
      ],
      # When glue is a dependency, it needs to be a hard dependency.
      # Dependents may rely on files generated by this target or one of its
      # own hard dependencies.
      'hard_dependency': 1,
      'export_dependent_settings': [
        'webcore',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
          'export_dependent_settings': [
            # Users of webcursor.h need the GTK include path.
            '../build/linux/system.gyp:gtk',
          ],
          'sources!': [
            'glue/plugins/plugin_stubs.cc',
          ],
        }, { # else: OS!="linux"
          'sources/': [['exclude', '_(linux|gtk)(_data)?\\.cc$'],
                       ['exclude', r'/gtk_']],
        }],
        ['OS!="mac"', {
          'sources/': [['exclude', '_mac\\.(cc|mm)$']]
        }],
        ['OS!="win"', {
          'sources/': [['exclude', '_win\\.cc$']],
          'sources!': [
            # TODO(port): Mac uses webplugin_delegate_impl_mac.cc and GTK uses
            # webplugin_delegate_impl_gtk.cc.  Seems like this should be
            # renamed webplugin_delegate_impl_win.cc, then we could get rid
            # of the explicit exclude.
            'glue/plugins/webplugin_delegate_impl.cc',

            # These files are Windows-only now but may be ported to other
            # platforms.
            'glue/glue_accessibility_object.cc',
            'glue/glue_accessibility_object.h',
            'glue/plugins/mozilla_extensions.cc',
            'glue/plugins/webplugin_delegate_impl.cc',
            'glue/webaccessibility.h',
            'glue/webaccessibilitymanager.h',
            'glue/webaccessibilitymanager_impl.cc',
            'glue/webaccessibilitymanager_impl.cc',
            'glue/webaccessibilitymanager_impl.h',
            'glue/webthemeengine_impl_win.cc',
          ],
        }, {  # else: OS=="win"
          'sources/': [['exclude', '_posix\\.cc$']],
          'dependencies': [
            '../build/win/system.gyp:cygwin',
            'activex_shim/activex_shim.gyp:activex_shim',
            'default_plugin/default_plugin.gyp:default_plugin',
          ],
          'sources!': [
            'glue/plugins/plugin_stubs.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'inspector_resources',
      'type': 'none',
      'msvs_guid': '5330F8EE-00F5-D65C-166E-E3150171055D',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/resources/inspector',
          'files': [
            'glue/devtools/js/base.js',
            'glue/devtools/js/debugger_agent.js',
            'glue/devtools/js/devtools.css',
            'glue/devtools/js/devtools.html',
            'glue/devtools/js/devtools.js',
            'glue/devtools/js/devtools_callback.js',
            'glue/devtools/js/devtools_host_stub.js',
            'glue/devtools/js/dom_agent.js',
            'glue/devtools/js/inject.js',
            'glue/devtools/js/inspector_controller.js',
            'glue/devtools/js/inspector_controller_impl.js',
            'glue/devtools/js/profiler_processor.js',
            'glue/devtools/js/tests.js',
            '../third_party/WebKit/WebCore/inspector/front-end/BottomUpProfileDataGridTree.js',
            '../third_party/WebKit/WebCore/inspector/front-end/Breakpoint.js',
            '../third_party/WebKit/WebCore/inspector/front-end/BreakpointsSidebarPane.js',
            '../third_party/WebKit/WebCore/inspector/front-end/CallStackSidebarPane.js',
            '../third_party/WebKit/WebCore/inspector/front-end/Console.js',
            '../third_party/WebKit/WebCore/inspector/front-end/Database.js',
            '../third_party/WebKit/WebCore/inspector/front-end/DatabaseQueryView.js',
            '../third_party/WebKit/WebCore/inspector/front-end/DatabasesPanel.js',
            '../third_party/WebKit/WebCore/inspector/front-end/DatabaseTableView.js',
            '../third_party/WebKit/WebCore/inspector/front-end/DataGrid.js',
            '../third_party/WebKit/WebCore/inspector/front-end/ElementsPanel.js',
            '../third_party/WebKit/WebCore/inspector/front-end/ElementsTreeOutline.js',
            '../third_party/WebKit/WebCore/inspector/front-end/FontView.js',
            '../third_party/WebKit/WebCore/inspector/front-end/ImageView.js',
            '../third_party/WebKit/WebCore/inspector/front-end/inspector.css',
            '../third_party/WebKit/WebCore/inspector/front-end/inspector.html',
            '../third_party/WebKit/WebCore/inspector/front-end/inspector.js',
            '../third_party/WebKit/WebCore/inspector/front-end/KeyboardShortcut.js',
            '../third_party/WebKit/WebCore/inspector/front-end/MetricsSidebarPane.js',
            '../third_party/WebKit/WebCore/inspector/front-end/Object.js',
            '../third_party/WebKit/WebCore/inspector/front-end/ObjectPropertiesSection.js',
            '../third_party/WebKit/WebCore/inspector/front-end/Panel.js',
            '../third_party/WebKit/WebCore/inspector/front-end/PanelEnablerView.js',
            '../third_party/WebKit/WebCore/inspector/front-end/Placard.js',
            '../third_party/WebKit/WebCore/inspector/front-end/ProfilesPanel.js',
            '../third_party/WebKit/WebCore/inspector/front-end/ProfileDataGridTree.js',
            '../third_party/WebKit/WebCore/inspector/front-end/ProfileView.js',
            '../third_party/WebKit/WebCore/inspector/front-end/PropertiesSection.js',
            '../third_party/WebKit/WebCore/inspector/front-end/PropertiesSidebarPane.js',
            '../third_party/WebKit/WebCore/inspector/front-end/Resource.js',
            '../third_party/WebKit/WebCore/inspector/front-end/ResourceCategory.js',
            '../third_party/WebKit/WebCore/inspector/front-end/ResourcesPanel.js',
            '../third_party/WebKit/WebCore/inspector/front-end/ResourceView.js',
            '../third_party/WebKit/WebCore/inspector/front-end/ScopeChainSidebarPane.js',
            '../third_party/WebKit/WebCore/inspector/front-end/Script.js',
            '../third_party/WebKit/WebCore/inspector/front-end/ScriptsPanel.js',
            '../third_party/WebKit/WebCore/inspector/front-end/ScriptView.js',
            '../third_party/WebKit/WebCore/inspector/front-end/SidebarPane.js',
            '../third_party/WebKit/WebCore/inspector/front-end/SidebarTreeElement.js',
            '../third_party/WebKit/WebCore/inspector/front-end/SourceFrame.js',
            '../third_party/WebKit/WebCore/inspector/front-end/SourceView.js',
            '../third_party/WebKit/WebCore/inspector/front-end/StylesSidebarPane.js',
            '../third_party/WebKit/WebCore/inspector/front-end/TextPrompt.js',
            '../third_party/WebKit/WebCore/inspector/front-end/TopDownProfileDataGridTree.js',
            '../third_party/WebKit/WebCore/inspector/front-end/treeoutline.js',
            '../third_party/WebKit/WebCore/inspector/front-end/utilities.js',
            '../third_party/WebKit/WebCore/inspector/front-end/View.js',
            '../v8/tools/codemap.js',
            '../v8/tools/consarray.js',
            '../v8/tools/csvparser.js',
            '../v8/tools/logreader.js',
            '../v8/tools/profile.js',
            '../v8/tools/profile_view.js',
            '../v8/tools/splaytree.js',            
          ],
        },
        {
          'destination': '<(PRODUCT_DIR)/resources/inspector/Images',
          'files': [
            '../third_party/WebKit/WebCore/inspector/front-end/Images/back.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/checker.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/clearConsoleButtons.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/closeButtons.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/consoleButtons.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/database.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/databasesIcon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/databaseTable.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/debuggerContinue.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/debuggerPause.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/debuggerStepInto.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/debuggerStepOut.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/debuggerStepOver.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/disclosureTriangleSmallDown.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/disclosureTriangleSmallDownBlack.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/disclosureTriangleSmallDownWhite.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/disclosureTriangleSmallRight.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/disclosureTriangleSmallRightBlack.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/disclosureTriangleSmallRightDown.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/disclosureTriangleSmallRightDownBlack.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/disclosureTriangleSmallRightDownWhite.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/disclosureTriangleSmallRightWhite.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/dockButtons.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/elementsIcon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/enableButtons.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/errorIcon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/errorMediumIcon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/excludeButtons.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/focusButtons.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/forward.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/glossyHeader.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/glossyHeaderPressed.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/glossyHeaderSelected.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/glossyHeaderSelectedPressed.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/goArrow.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/graphLabelCalloutLeft.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/graphLabelCalloutRight.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/largerResourcesButtons.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/nodeSearchButtons.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/paneBottomGrow.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/paneBottomGrowActive.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/paneGrowHandleLine.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/pauseOnExceptionButtons.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/percentButtons.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/profileGroupIcon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/profileIcon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/profilesIcon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/profileSmallIcon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/profilesSilhouette.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/radioDot.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/recordButtons.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/reloadButtons.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/resourceCSSIcon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/resourceDocumentIcon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/resourceDocumentIconSmall.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/resourceJSIcon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/resourcePlainIcon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/resourcePlainIconSmall.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/resourcesIcon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/resourcesSilhouette.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/resourcesSizeGraphIcon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/resourcesTimeGraphIcon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/scriptsIcon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/scriptsSilhouette.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/searchSmallBlue.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/searchSmallBrightBlue.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/searchSmallGray.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/searchSmallWhite.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/segment.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/segmentEnd.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/segmentHover.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/segmentHoverEnd.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/segmentSelected.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/segmentSelectedEnd.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/splitviewDimple.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/splitviewDividerBackground.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/statusbarBackground.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/statusbarBottomBackground.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/statusbarButtons.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/statusbarMenuButton.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/statusbarMenuButtonSelected.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/statusbarResizerHorizontal.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/statusbarResizerVertical.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/timelineHollowPillBlue.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/timelineHollowPillGray.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/timelineHollowPillGreen.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/timelineHollowPillOrange.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/timelineHollowPillPurple.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/timelineHollowPillRed.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/timelineHollowPillYellow.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/timelinePillBlue.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/timelinePillGray.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/timelinePillGreen.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/timelinePillOrange.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/timelinePillPurple.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/timelinePillRed.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/timelinePillYellow.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/tipBalloon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/tipBalloonBottom.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/tipIcon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/tipIconPressed.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/toolbarItemSelected.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/treeDownTriangleBlack.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/treeDownTriangleWhite.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/treeRightTriangleBlack.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/treeRightTriangleWhite.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/treeUpTriangleBlack.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/treeUpTriangleWhite.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/userInputIcon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/userInputPreviousIcon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/warningIcon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/warningMediumIcon.png',
            '../third_party/WebKit/WebCore/inspector/front-end/Images/warningsErrors.png',
          ],
        },
      ],
    },
  ],
}
