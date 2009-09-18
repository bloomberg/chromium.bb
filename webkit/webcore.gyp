# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'features.gypi',
    '../third_party/WebKit/WebCore/WebCore.gypi',
  ],
  'variables': {
    'webcore_include_dirs': [
      '../third_party/WebKit/WebCore',
      '../third_party/WebKit/WebCore/accessibility',
      '../third_party/WebKit/WebCore/accessibility/chromium',
      '../third_party/WebKit/WebCore/bindings/v8',
      '../third_party/WebKit/WebCore/bindings/v8/custom',
      '../third_party/WebKit/WebCore/bridge',
      '../third_party/WebKit/WebCore/css',
      '../third_party/WebKit/WebCore/dom',
      '../third_party/WebKit/WebCore/dom/default',
      '../third_party/WebKit/WebCore/editing',
      '../third_party/WebKit/WebCore/history',
      '../third_party/WebKit/WebCore/html',
      '../third_party/WebKit/WebCore/html/canvas',
      '../third_party/WebKit/WebCore/inspector',
      '../third_party/WebKit/WebCore/loader',
      '../third_party/WebKit/WebCore/loader/appcache',
      '../third_party/WebKit/WebCore/loader/archive',
      '../third_party/WebKit/WebCore/loader/icon',
      '../third_party/WebKit/WebCore/notifications',
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
      '../third_party/WebKit/WebCore/platform/mock',
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
      ['OS=="mac"', {
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
        'webcore_include_dirs': [
          '../third_party/WebKit/WebCore/page/win',
          '../third_party/WebKit/WebCore/platform/graphics/win',
          '../third_party/WebKit/WebCore/platform/text/win',
          '../third_party/WebKit/WebCore/platform/win',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'webcore',
      'type': '<(library)',
      'msvs_guid': '1C16337B-ACF3-4D03-AA90-851C5B5EADA6',
      'dependencies': [
        'config.gyp:config',
        'javascriptcore.gyp:pcre',
        'javascriptcore.gyp:wtf',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../skia/skia.gyp:skia',
        '../third_party/libjpeg/libjpeg.gyp:libjpeg',
        '../third_party/libpng/libpng.gyp:libpng',
        '../third_party/libxml/libxml.gyp:libxml',
        '../third_party/libxslt/libxslt.gyp:libxslt',
        '../third_party/npapi/npapi.gyp:npapi',
        '../third_party/sqlite/sqlite.gyp:sqlite',
      ],
      'defines': [
        '<@(feature_defines)',
        '<@(non_feature_defines)',
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
              '--include', '../third_party/WebKit/WebCore/notifications',
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

        # This file includes all the .cpp files generated from the .idl files
        # in webcore_files.
        '../third_party/WebKit/WebCore/bindings/v8/DerivedSourcesAllInOne.cpp',

        '<@(webcore_files)',

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
        ['exclude', '/(android|cairo|cf|cg|curl|gtk|haiku|linux|mac|opentype|posix|qt|soup|symbian|win|wx)/'],
        ['exclude', '(?<!Chromium)(SVGAllInOne|Android|Cairo|CF|CG|Curl|Gtk|Linux|Mac|OpenType|POSIX|Posix|Qt|Safari|Soup|Symbian|Win|Wx)\\.(cpp|mm?)$'],

        # JSC-only.
        ['exclude', '/third_party/WebKit/WebCore/inspector/JavaScript[^/]*\\.cpp$'],

        # ENABLE_OFFLINE_WEB_APPLICATIONS, exclude most of webcore's impl
        ['exclude', '/third_party/WebKit/WebCore/loader/appcache/'],
        ['include', '/third_party/WebKit/WebCore/loader/appcache/ApplicationCacheHost\.h$'],
        ['include', '/third_party/WebKit/WebCore/loader/appcache/DOMApplicationCache\.(h|cpp|idl)$'],

        # SVG_FILTERS only.
        ['exclude', '/third_party/WebKit/WebCore/(platform|svg)/graphics/filters/'],
        ['exclude', '/third_party/WebKit/WebCore/svg/Filter[^/]*\\.cpp$'],
        ['exclude', '/third_party/WebKit/WebCore/svg/SVG(FE|Filter)[^/]*\\.cpp$'],

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

        # Theme.cpp is used only if we're using USE_NEW_THEME. We are not for
        # Windows and Linux. We manually include Theme.cpp for the Mac below.
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

        # We use a multi-process version from the WebKit API.
        '../third_party/WebKit/WebCore/dom/default/PlatformMessagePortChannel.cpp',
        '../third_party/WebKit/WebCore/dom/default/PlatformMessagePortChannel.h',

      ],
      'direct_dependent_settings': {
        # webkit.gyp:webkit & glue need the same feature defines too
        #'defines': [ 
        #  '<@(feature_defines)',
        #  '<@(non_feature_defines)',
        #],
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
        'javascriptcore.gyp:wtf',
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
        ['OS=="linux" or OS=="freebsd"', {
          'dependencies': [
            '../build/linux/system.gyp:fontconfig',
            '../build/linux/system.gyp:gtk',
          ],
          'sources': [
            '../third_party/WebKit/WebCore/platform/graphics/chromium/VDMXParser.cpp',
            '../third_party/WebKit/WebCore/platform/graphics/chromium/HarfbuzzSkia.cpp',
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
            ['include', '/third_party/WebKit/WebCore/platform/graphics/mac/FloatPointMac\\.mm$'],
            ['include', '/third_party/WebKit/WebCore/platform/graphics/mac/FloatRectMac\\.mm$'],
            ['include', '/third_party/WebKit/WebCore/platform/graphics/mac/FloatSizeMac\\.mm$'],
            ['include', '/third_party/WebKit/WebCore/platform/graphics/mac/GlyphPageTreeNodeMac\\.cpp$'],
            ['include', '/third_party/WebKit/WebCore/platform/graphics/mac/GraphicsContextMac\\.mm$'],
            ['include', '/third_party/WebKit/WebCore/platform/graphics/mac/IntRectMac\\.mm$'],
            ['include', '/third_party/WebKit/WebCore/platform/mac/BlockExceptions\\.mm$'],
            ['include', '/third_party/WebKit/WebCore/platform/mac/LocalCurrentGraphicsContext\\.mm$'],
            ['include', '/third_party/WebKit/WebCore/platform/mac/PurgeableBufferMac\\.cpp$'],
            ['include', '/third_party/WebKit/WebCore/platform/mac/ScrollbarThemeMac\\.mm$'],
            ['include', '/third_party/WebKit/WebCore/platform/mac/WebCoreSystemInterface\\.mm$'],
            ['include', '/third_party/WebKit/WebCore/platform/mac/WebCoreTextRenderer\\.mm$'],
            ['include', '/third_party/WebKit/WebCore/platform/text/mac/ShapeArabic\\.c$'],
            ['include', '/third_party/WebKit/WebCore/platform/text/mac/String(Impl)?Mac\\.mm$'],
            # Use USE_NEW_THEME on Mac.
            ['include', '/third_party/WebKit/WebCore/platform/Theme\\.cpp$'],

            ['include', '/third_party/WebKit/WebKit/mac/WebCoreSupport/WebSystemInterface\\.m$'],
          ],
          'sources!': [
            # The Mac currently uses FontCustomPlatformData.cpp from
            # platform/graphics/mac, included by regex above, instead.
            '../third_party/WebKit/WebCore/platform/graphics/chromium/FontCustomPlatformData.cpp',

            # The Mac currently uses ScrollbarThemeMac.mm, included by regex
            # above, instead of ScrollbarThemeChromium.cpp.
            '../third_party/WebKit/WebCore/platform/chromium/ScrollbarThemeChromium.cpp',

            # The Mac uses ImageSourceCG.cpp from platform/graphics/cg, included
            # by regex above, instead.
            '../third_party/WebKit/WebCore/platform/graphics/ImageSource.cpp',

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
          ],
          # This is needed because Event.h in this directory is blocked
          # by a system header on windows.
          'include_dirs++': ['../third_party/WebKit/WebCore/dom'],
          'direct_dependent_settings': {
            'include_dirs+++': ['../third_party/WebKit/WebCore/dom'],
          },
        }],
        ['OS!="linux" and OS!="freebsd"', {'sources/': [['exclude', '(Gtk|Linux)\\.cpp$']]}],
        ['OS!="mac"', {'sources/': [['exclude', 'Mac\\.(cpp|mm?)$']]}],
        ['OS!="win"', {
          'sources/': [
            ['exclude', 'Win\\.cpp$'],
            ['exclude', '/(Windows|Uniscribe)[^/]*\\.cpp$']
          ],
        }],
      ],
    },
  ], # targets
}
