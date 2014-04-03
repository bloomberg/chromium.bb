#
# Copyright (C) 2009 Google Inc. All rights reserved.
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
    '../build/win/precompile.gypi',
    '../build/features.gypi',
    '../build/scripts/scripts.gypi',
    '../modules/modules.gypi',
    '../bindings/bindings.gypi',
    'core.gypi',
  ],

  'variables': {
    'enable_wexit_time_destructors': 1,

    'webcore_include_dirs': [
      '../..',
      '..',
      '<(SHARED_INTERMEDIATE_DIR)/blink',
      '<(bindings_output_dir)',
    ],

    'conditions': [
      ['OS=="android" and use_openmax_dl_fft!=0', {
        'webcore_include_dirs': [
          '<(DEPTH)/third_party/openmax_dl'
        ]
      }],
    ],
  },  # variables

  'target_defaults': {
    'variables': {
      'optimize': 'max',
    },
  },

  'targets': [
    {
      'target_name': 'inspector_protocol_sources',
      'type': 'none',
      'dependencies': [
        'generate_inspector_protocol_version'
      ],
      'actions': [
        {
          'action_name': 'generateInspectorProtocolBackendSources',
          'inputs': [
            # The python script in action below.
            'inspector/CodeGeneratorInspector.py',
            # The helper script imported by CodeGeneratorInspector.py.
            'inspector/CodeGeneratorInspectorStrings.py',
            # Input file for the script.
            '../devtools/protocol.json',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorBackendDispatcher.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorBackendDispatcher.h',
            '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorFrontend.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorFrontend.h',
            '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorTypeBuilder.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorTypeBuilder.h',
          ],
          'variables': {
            'generator_include_dirs': [
            ],
          },
          'action': [
            'python',
            'inspector/CodeGeneratorInspector.py',
            '../devtools/protocol.json',
            '--output_dir', '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
          'message': 'Generating Inspector protocol backend sources from protocol.json',
        },
      ]
    },
    {
      'target_name': 'inspector_instrumentation_sources',
      'type': 'none',
      'dependencies': [],
      'actions': [
        {
          'action_name': 'generateInspectorInstrumentation',
          'inputs': [
            # The python script in action below.
            'inspector/CodeGeneratorInstrumentation.py',
            # Input file for the script.
            'inspector/InspectorInstrumentation.idl',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorCanvasInstrumentationInl.h',
            '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorConsoleInstrumentationInl.h',
            '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorInstrumentationInl.h',
            '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorOverridesInl.h',
            '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorPromiseInstrumentationInl.h',
            '<(SHARED_INTERMEDIATE_DIR)/blink/InstrumentingAgentsInl.h',
            '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorInstrumentationImpl.cpp',
          ],
          'action': [
            'python',
            'inspector/CodeGeneratorInstrumentation.py',
            'inspector/InspectorInstrumentation.idl',
            '--output_dir', '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
          'message': 'Generating Inspector instrumentation code from InspectorInstrumentation.idl',
        }
      ]
    },
    {
      'target_name': 'generate_inspector_protocol_version',
      'type': 'none',
      'actions': [
         {
          'action_name': 'generateInspectorProtocolVersion',
          'inputs': [
            'inspector/generate-inspector-protocol-version',
            '../devtools/protocol.json',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorProtocolVersion.h',
          ],
          'variables': {
            'generator_include_dirs': [
            ],
          },
          'action': [
            'python',
            'inspector/generate-inspector-protocol-version',
            '-o',
            '<@(_outputs)',
            '<@(_inputs)'
          ],
          'message': 'Validate inspector protocol for backwards compatibility and generate version file',
        }
      ]
    },
    {
      'target_name': 'inspector_overlay_page',
      'type': 'none',
      'variables': {
        'input_file_path': 'inspector/InspectorOverlayPage.html',
        'output_file_path': '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorOverlayPage.h',
        'character_array_name': 'InspectorOverlayPage_html',
      },
      'includes': [ '../build/ConvertFileToHeaderWithCharacterArray.gypi' ],
    },
    {
      'target_name': 'injected_canvas_script_source',
      'type': 'none',
      'variables': {
        'input_file_path': 'inspector/InjectedScriptCanvasModuleSource.js',
        'output_file_path': '<(SHARED_INTERMEDIATE_DIR)/blink/InjectedScriptCanvasModuleSource.h',
        'character_array_name': 'InjectedScriptCanvasModuleSource_js',
      },
      'includes': [ '../build/ConvertFileToHeaderWithCharacterArray.gypi' ],
    },
    {
      'target_name': 'injected_script_source',
      'type': 'none',
      'variables': {
        'input_file_path': 'inspector/InjectedScriptSource.js',
        'output_file_path': '<(SHARED_INTERMEDIATE_DIR)/blink/InjectedScriptSource.h',
        'character_array_name': 'InjectedScriptSource_js',
      },
      'includes': [ '../build/ConvertFileToHeaderWithCharacterArray.gypi' ],
    },
    {
      'target_name': 'debugger_script_source',
      'type': 'none',
      'variables': {
        'input_file_path': '<(bindings_v8_dir)/DebuggerScript.js',
        'output_file_path': '<(SHARED_INTERMEDIATE_DIR)/blink/DebuggerScriptSource.h',
        'character_array_name': 'DebuggerScriptSource_js',
      },
      'includes': [ '../build/ConvertFileToHeaderWithCharacterArray.gypi' ],
    },
    {
      'target_name': 'webcore_generated',
      'type': 'static_library',
      'hard_dependency': 1,
      'dependencies': [
        'webcore_prerequisites',
        'core_generated.gyp:make_core_generated',
        'inspector_overlay_page',
        'inspector_protocol_sources',
        'inspector_instrumentation_sources',
        'injected_canvas_script_source',
        'injected_script_source',
        'debugger_script_source',
        '../bindings/generated_bindings.gyp:generated_bindings',
        '../platform/platform_generated.gyp:make_platform_generated',
        '../wtf/wtf.gyp:wtf',
        '<(DEPTH)/gin/gin.gyp:gin',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/iccjpeg/iccjpeg.gyp:iccjpeg',
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
        '<(DEPTH)/third_party/libwebp/libwebp.gyp:libwebp',
        '<(DEPTH)/third_party/libxml/libxml.gyp:libxml',
        '<(DEPTH)/third_party/libxslt/libxslt.gyp:libxslt',
        '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
        '<(DEPTH)/third_party/qcms/qcms.gyp:qcms',
        '<(DEPTH)/third_party/sqlite/sqlite.gyp:sqlite',
        '<(DEPTH)/url/url.gyp:url_lib',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
      ],
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)/blink',
        '<(bindings_output_dir)',
        '<@(webcore_include_dirs)',

        # FIXME: Remove these once the bindings script generates qualified
        # includes for these correctly. (Sequences don't work yet.)
        '<(bindings_v8_dir)/custom',
        'html',
        'html/shadow',
        'inspector',
        'svg',
      ],
      'sources': [
        # These files include all the .cpp files generated from the .idl files
        # in webcore_files.
        '<@(aggregate_generated_bindings_files)',
        '<@(bindings_files)',

        # Additional .cpp files for HashTools.h
        '<(SHARED_INTERMEDIATE_DIR)/blink/CSSPropertyNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/blink/CSSValueKeywords.cpp',

        # Additional .cpp files from make_core_generated actions.
        '<(SHARED_INTERMEDIATE_DIR)/blink/Event.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/blink/EventHeaders.h',
        '<(SHARED_INTERMEDIATE_DIR)/blink/EventInterfaces.h',
        '<(SHARED_INTERMEDIATE_DIR)/blink/EventNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/blink/EventNames.h',
        '<(SHARED_INTERMEDIATE_DIR)/blink/EventTargetHeaders.h',
        '<(SHARED_INTERMEDIATE_DIR)/blink/EventTargetInterfaces.h',
        '<(SHARED_INTERMEDIATE_DIR)/blink/EventTargetNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/blink/EventTargetNames.h',
        '<(SHARED_INTERMEDIATE_DIR)/blink/EventTypeNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/blink/EventTypeNames.h',
        '<(SHARED_INTERMEDIATE_DIR)/blink/FetchInitiatorTypeNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/blink/HTMLElementFactory.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/blink/HTMLElementFactory.h',
        '<(SHARED_INTERMEDIATE_DIR)/blink/HTMLElementLookupTrie.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/blink/HTMLElementLookupTrie.h',
        '<(SHARED_INTERMEDIATE_DIR)/blink/HTMLNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/blink/InputTypeNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/blink/MathMLNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/blink/SVGNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/blink/UserAgentStyleSheetsData.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/blink/V8HTMLElementWrapperFactory.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/blink/XLinkNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/blink/XMLNSNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/blink/XMLNames.cpp',

        # Generated from HTMLEntityNames.in
        '<(SHARED_INTERMEDIATE_DIR)/blink/HTMLEntityTable.cpp',

        # Generated from MediaFeatureNames.in
        '<(SHARED_INTERMEDIATE_DIR)/blink/MediaFeatureNames.cpp',

        # Generated from MediaTypeNames.in
        '<(SHARED_INTERMEDIATE_DIR)/blink/MediaTypeNames.cpp',

        # Generated from CSSTokenizer-in.cpp
        '<(SHARED_INTERMEDIATE_DIR)/blink/CSSTokenizer.cpp',

        # Generated from BisonCSSParser-in.cpp
        '<(SHARED_INTERMEDIATE_DIR)/blink/BisonCSSParser.cpp',

        # Generated from HTMLMetaElement-in.cpp
        '<(SHARED_INTERMEDIATE_DIR)/blink/HTMLMetaElement.cpp',

        # Additional .cpp files from the make_core_generated rules.
        '<(SHARED_INTERMEDIATE_DIR)/blink/CSSGrammar.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/blink/XPathGrammar.cpp',

        # Additional .cpp files from the inspector_protocol_sources list.
        '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorFrontend.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorBackendDispatcher.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorTypeBuilder.cpp',

        # Additional .cpp files from the inspector_instrumentation_sources list.
        '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorCanvasInstrumentationInl.h',
        '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorConsoleInstrumentationInl.h',
        '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorInstrumentationInl.h',
        '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorOverridesInl.h',
        '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorPromiseInstrumentationInl.h',
        '<(SHARED_INTERMEDIATE_DIR)/blink/InstrumentingAgentsInl.h',
        '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorInstrumentationImpl.cpp',

        # Additional .cpp files for SVG.
        '<(SHARED_INTERMEDIATE_DIR)/blink/SVGElementFactory.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/blink/V8SVGElementWrapperFactory.cpp',

        # Generated from make_style_shorthands.py
        '<(SHARED_INTERMEDIATE_DIR)/blink/StylePropertyShorthand.cpp',

        # Generated from make_style_builder.py
        '<(SHARED_INTERMEDIATE_DIR)/blink/StyleBuilder.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/blink/StyleBuilderFunctions.cpp',
      ],
      'conditions': [
        ['OS=="win" and component=="shared_library"', {
          'defines': [
            'USING_V8_SHARED',
          ],
        }],
        ['OS=="win"', {
          # In generated bindings code: 'switch contains default but no case'.
          # Disable c4267 warnings until we fix size_t to int truncations.
          # 4702 is disabled because of issues in Bison-generated
          # XPathGrammar.cpp and CSSGrammar.cpp.
          'msvs_disabled_warnings': [ 4065, 4267, 4702 ],
        }],
        ['OS in ("linux", "android") and "WTF_USE_WEBAUDIO_IPP=1" in feature_defines', {
          'cflags': [
            '<!@(pkg-config --cflags-only-I ipp)',
          ],
        }],
      ],
    },
    {
      # We'll soon split libwebcore in multiple smaller libraries.
      # webcore_prerequisites will be the 'base' target of every sub-target.
      'target_name': 'webcore_prerequisites',
      'type': 'none',
      'dependencies': [
        'debugger_script_source',
        'injected_canvas_script_source',
        'injected_script_source',
        'inspector_overlay_page',
        'inspector_protocol_sources',
        'inspector_instrumentation_sources',
        '../bindings/generated_bindings.gyp:generated_bindings',
        'core_generated.gyp:make_core_generated',
        '../wtf/wtf.gyp:wtf',
        '../config.gyp:config',
        '../platform/blink_platform.gyp:blink_platform',
        '<(DEPTH)/gpu/gpu.gyp:gles2_c_lib',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(angle_path)/src/build_angle.gyp:translator',
        '<(DEPTH)/third_party/iccjpeg/iccjpeg.gyp:iccjpeg',
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
        '<(DEPTH)/third_party/libwebp/libwebp.gyp:libwebp',
        '<(DEPTH)/third_party/libxml/libxml.gyp:libxml',
        '<(DEPTH)/third_party/libxslt/libxslt.gyp:libxslt',
        '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
        '<(DEPTH)/third_party/ots/ots.gyp:ots',
        '<(DEPTH)/third_party/qcms/qcms.gyp:qcms',
        '<(DEPTH)/third_party/sqlite/sqlite.gyp:sqlite',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
        '<(DEPTH)/url/url.gyp:url_lib',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
      ],
      'export_dependent_settings': [
        '../wtf/wtf.gyp:wtf',
        '../config.gyp:config',
        '<(DEPTH)/gpu/gpu.gyp:gles2_c_lib',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(angle_path)/src/build_angle.gyp:translator',
        '<(DEPTH)/third_party/iccjpeg/iccjpeg.gyp:iccjpeg',
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
        '<(DEPTH)/third_party/libwebp/libwebp.gyp:libwebp',
        '<(DEPTH)/third_party/libxml/libxml.gyp:libxml',
        '<(DEPTH)/third_party/libxslt/libxslt.gyp:libxslt',
        '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
        '<(DEPTH)/third_party/ots/ots.gyp:ots',
        '<(DEPTH)/third_party/qcms/qcms.gyp:qcms',
        '<(DEPTH)/third_party/sqlite/sqlite.gyp:sqlite',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
        '<(DEPTH)/url/url.gyp:url_lib',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
      ],
      'direct_dependent_settings': {
        'defines': [
          'BLINK_IMPLEMENTATION=1',
          'INSIDE_BLINK',
        ],
        'include_dirs': [
          '<@(webcore_include_dirs)',
          '<(DEPTH)/gpu',
          '<(angle_path)/include',
        ],
        'xcode_settings': {
          # Some Mac-specific parts of WebKit won't compile without having this
          # prefix header injected.
          # FIXME: make this a first-class setting.
          'GCC_PREFIX_HEADER': 'WebCorePrefixMac.h',
        },
      },
      'conditions': [
        ['OS=="win" and component=="shared_library"', {
          'direct_dependent_settings': {
            'defines': [
               'USING_V8_SHARED',
            ],
          },
        }],
        ['use_x11 == 1', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:fontconfig',
          ],
          'export_dependent_settings': [
            '<(DEPTH)/build/linux/system.gyp:fontconfig',
          ],
          'direct_dependent_settings': {
            'cflags': [
              # WebCore does not work with strict aliasing enabled.
              # https://bugs.webkit.org/show_bug.cgi?id=25864
              '-fno-strict-aliasing',
            ],
          },
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:gtk',
          ],
          'export_dependent_settings': [
            '<(DEPTH)/build/linux/system.gyp:gtk',
          ],
        }],
        ['OS=="android"', {
          'sources/': [
            ['exclude', 'accessibility/'],
          ],
        }],
        ['OS in ("linux", "android") and "WTF_USE_WEBAUDIO_IPP=1" in feature_defines', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(pkg-config --cflags-only-I ipp)',
            ],
          },
        }],
        ['OS=="mac"', {
          'direct_dependent_settings': {
            'defines': [
              # Chromium's version of WebCore includes the following Objective-C
              # classes. The system-provided WebCore framework may also provide
              # these classes. Because of the nature of Objective-C binding
              # (dynamically at runtime), it's possible for the
              # Chromium-provided versions to interfere with the system-provided
              # versions.  This may happen when a system framework attempts to
              # use core.framework, such as when converting an HTML-flavored
              # string to an NSAttributedString.  The solution is to force
              # Objective-C class names that would conflict to use alternate
              # names.
              #
              # This list will hopefully shrink but may also grow.  Its
              # performance is monitored by the "Check Objective-C Rename"
              # postbuild step, and any suspicious-looking symbols not handled
              # here or whitelisted in that step will cause a build failure.
              #
              # If this is unhandled, the console will receive log messages
              # such as:
              # com.google.Chrome[] objc[]: Class ScrollbarPrefsObserver is implemented in both .../Google Chrome.app/Contents/Versions/.../Google Chrome Helper.app/Contents/MacOS/../../../Google Chrome Framework.framework/Google Chrome Framework and /System/Library/Frameworks/WebKit.framework/Versions/A/Frameworks/WebCore.framework/Versions/A/WebCore. One of the two will be used. Which one is undefined.
              'WebCoreTextFieldCell=ChromiumWebCoreObjCWebCoreTextFieldCell',
              'WebCoreRenderThemeNotificationObserver=ChromiumWebCoreObjCWebCoreRenderThemeNotificationObserver',
            ],
            'postbuilds': [
              {
                # This step ensures that any Objective-C names that aren't
                # redefined to be "safe" above will cause a build failure.
                'postbuild_name': 'Check Objective-C Rename',
                'variables': {
                  'class_whitelist_regex':
                      'ChromiumWebCoreObjC|TCMVisibleView|RTCMFlippedView|ScrollerStyleObserver',
                  'category_whitelist_regex':
                      'TCMInterposing|ScrollAnimatorChromiumMacExt|WebCoreTheme',
                },
                'action': [
                  '../build/scripts/check_objc_rename.sh',
                  '<(class_whitelist_regex)',
                  '<(category_whitelist_regex)',
                ],
              },
            ],
          },
        }],
        ['"WTF_USE_WEBAUDIO_FFMPEG=1" in feature_defines', {
          # This directory needs to be on the include path for multiple sub-targets of webcore.
          'direct_dependent_settings': {
            'include_dirs': [
              '<(DEPTH)/third_party/ffmpeg',
            ],
          },
          'dependencies': [
            '<(DEPTH)/third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
          ],
        }],
       ['"WTF_USE_WEBAUDIO_OPENMAX_DL_FFT=1" in feature_defines', {
         'direct_dependent_settings': {
           'include_dirs': [
             '<(DEPTH)/third_party/openmax_dl',
           ],
         },
         'dependencies': [
           '<(DEPTH)/third_party/openmax_dl/dl/dl.gyp:openmax_dl',
         ],
       }],
        # Windows shared builder needs extra help for linkage
        ['OS=="win" and "WTF_USE_WEBAUDIO_FFMPEG=1" in feature_defines', {
          'export_dependent_settings': [
            '<(DEPTH)/third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
          ],
        }],
      ],
    },
    {
      'target_name': 'webcore_dom',
      'type': 'static_library',
      'dependencies': [
        'webcore_prerequisites',
      ],
      'sources': [
        '<@(webcore_dom_files)',
      ],
      # Disable c4267 warnings until we fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      'target_name': 'webcore_html',
      'type': 'static_library',
      'dependencies': [
        'webcore_prerequisites',
      ],
      'sources': [
        '<@(webcore_html_files)',
      ],
      'conditions': [
        # Shard this taret into parts to work around linker limitations.
        # on link time code generation builds.
        ['OS=="win" and buildtype=="Official"', {
          'msvs_shard': 5,
        }],
        ['OS!="android"', {
          'sources/': [
            ['exclude', 'Android\\.cpp$'],
          ],
        }],
      ],
    },
    {
      'target_name': 'webcore_svg',
      'type': 'static_library',
      'dependencies': [
        'webcore_prerequisites',
      ],
      'sources': [
        '<@(webcore_svg_files)',
      ],
    },
    {
      'target_name': 'webcore_rendering',
      'type': 'static_library',
      'dependencies': [
        'webcore_prerequisites',
      ],
      'sources': [
        '<@(webcore_files)',
      ],
      'sources/': [
        ['exclude', '.*'],
        ['include', 'rendering/'],

        # FIXME: Figure out how to store these patterns in a variable.
        ['exclude', '(cf|cg|mac|opentype|svg|win)/'],
        ['exclude', '(?<!Chromium)(CF|CG|Mac|Win)\\.(cpp|mm?)$'],
        # Previous rule excludes things like ChromiumFooWin, include those.
        ['include', 'rendering/.*Chromium.*\\.(cpp|mm?)$'],
      ],
      'conditions': [
        # Shard this taret into parts to work around linker limitations.
        # on link time code generation builds.
        ['OS=="win" and buildtype=="Official"', {
          'msvs_shard': 5,
        }],
        ['use_default_render_theme==0', {
          'sources/': [
            ['exclude', 'rendering/RenderThemeChromiumDefault.*'],
          ],
        }],
        ['OS=="win"', {
          'sources/': [
            ['exclude', 'Posix\\.cpp$'],
          ],
        },{ # OS!="win"
          'sources/': [
            ['exclude', 'Win\\.cpp$'],
          ],
        }],
        ['OS=="win" and chromium_win_pch==1', {
          'sources/': [
            ['include', '<(DEPTH)/third_party/WebKit/Source/build/win/Precompile.cpp'],
          ],
        }],
        ['OS=="mac"', {
          'sources/': [
            # RenderThemeChromiumSkia is not used on mac since RenderThemeChromiumMac
            # does not reference the Skia code that is used by Windows, Linux and Android.
            ['exclude', 'rendering/RenderThemeChromiumSkia\\.cpp$'],
            # RenderThemeChromiumFontProvider is used by RenderThemeChromiumSkia.
            ['exclude', 'rendering/RenderThemeChromiumFontProvider\\.cpp'],
            ['exclude', 'rendering/RenderThemeChromiumFontProvider\\.h'],
          ],
        },{ # OS!="mac"
          'sources/': [['exclude', 'Mac\\.(cpp|mm?)$']]
        }],
        ['OS == "android" and target_arch == "ia32" and gcc_version == 46', {
          # Due to a bug in gcc 4.6 in android NDK, we get warnings about uninitialized variable.
          'cflags': ['-Wno-uninitialized'],
        }],
        ['OS != "linux"', {
          'sources/': [
            ['exclude', 'Linux\\.cpp$'],
          ],
        }],
        ['toolkit_uses_gtk == 0', {
          'sources/': [
            ['exclude', 'Gtk\\.cpp$'],
          ],
        }],
        ['OS=="android"', {
          'sources/': [
            ['include', 'rendering/RenderThemeChromiumFontProviderLinux\\.cpp$'],
            ['include', 'rendering/RenderThemeChromiumDefault\\.cpp$'],
          ],
        },{ # OS!="android"
          'sources/': [
            ['exclude', 'Android\\.cpp$'],
          ],
        }],
      ],
    },
    {
      'target_name': 'webcore_remaining',
      'type': 'static_library',
      'dependencies': [
        'webcore_prerequisites',
      ],
      'sources': [
        '<@(webcore_files)',
      ],
      'sources/': [
        ['exclude', 'rendering/'],

        # FIXME: Figure out how to store these patterns in a variable.
        ['exclude', '(cf|cg|mac|opentype|svg|win)/'],
        ['exclude', '(?<!Chromium)(CF|CG|Mac|Win)\\.(cpp|mm?)$'],
      ],
      'conditions': [
        # Shard this taret into parts to work around linker limitations.
        # on link time code generation builds.
        ['OS=="win" and buildtype=="Official"', {
          'msvs_shard': 19,
        }],
        ['OS != "linux"', {
          'sources/': [
            ['exclude', 'Linux\\.cpp$'],
          ],
        }],
        ['toolkit_uses_gtk == 0', {
          'sources/': [
            ['exclude', 'Gtk\\.cpp$'],
          ],
        }],
        ['OS=="android"', {
          'cflags': [
            # WebCore does not work with strict aliasing enabled.
            # https://bugs.webkit.org/show_bug.cgi?id=25864
            '-fno-strict-aliasing',
          ],
        }, { # OS!="android"
          'sources/': [['exclude', 'Android\\.cpp$']]
        }],
        ['OS=="mac"', {
          'sources': [
            'editing/SmartReplaceCF.cpp',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
            ],
          },
          'sources/': [
            # Additional files from the WebCore Mac build that are presently
            # used in the WebCore Chromium Mac build too.

            # The Mac build is USE(CF).
            ['include', 'CF\\.cpp$'],

            # Cherry-pick some files that can't be included by broader regexps.
            # Some of these are used instead of Chromium platform files, see
            # the specific exclusions in the "exclude" list below.
            ['include', 'platform/mac/WebCoreSystemInterface\\.h$'],
            ['include', 'platform/mac/WebCoreTextRenderer\\.mm$'],
            ['include', 'platform/text/mac/ShapeArabic\\.c$'],
            ['include', 'platform/text/mac/String(Impl)?Mac\\.mm$'],
            # Use USE_NEW_THEME on Mac.
            ['include', 'platform/Theme\\.cpp$'],
          ],
        }, { # OS!="mac"
          'sources/': [['exclude', 'Mac\\.(cpp|mm?)$']]
        }],
        ['OS=="win" and chromium_win_pch==1', {
          'sources/': [
            ['include', '<(DEPTH)/third_party/WebKit/Source/build/win/Precompile.cpp'],
          ],
        }],
      ],
      # Disable c4267 warnings until we fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, 4334, ],
    },
    {
      'target_name': 'webcore',
      'type': 'none',
      'dependencies': [
        'webcore_dom',
        'webcore_html',
        'webcore_remaining',
        'webcore_rendering',
        'webcore_svg',
        # Exported.
        'webcore_generated',
        '../wtf/wtf.gyp:wtf',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
        '<(DEPTH)/third_party/qcms/qcms.gyp:qcms',
        '<(DEPTH)/url/url.gyp:url_lib',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
      ],
      'export_dependent_settings': [
        '../wtf/wtf.gyp:wtf',
        'webcore_generated',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
        '<(DEPTH)/third_party/qcms/qcms.gyp:qcms',
        '<(DEPTH)/url/url.gyp:url_lib',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<@(webcore_include_dirs)',
        ],
      },
      'conditions': [
        ['OS=="linux" and "WTF_USE_WEBAUDIO_IPP=1" in feature_defines', {
          'link_settings': {
            'ldflags': [
              '<!@(pkg-config --libs-only-L ipp)',
            ],
            'libraries': [
              '-lipps -lippcore',
            ],
          },
        }],
        # Use IPP static libraries for x86 Android.
        ['OS=="android" and "WTF_USE_WEBAUDIO_IPP=1" in feature_defines', {
          'link_settings': {
            'libraries': [
               '<!@(pkg-config --libs ipp|sed s/-L//)/libipps_l.a',
               '<!@(pkg-config --libs ipp|sed s/-L//)/libippcore_l.a',
            ]
          },
        }],
      ],
    },
    {
      'target_name': 'webcore_testing',
      'type': 'static_library',
      'dependencies': [
        '../config.gyp:config',
        'webcore',
      ],
      'defines': [
        'BLINK_IMPLEMENTATION=1',
        'INSIDE_BLINK',
      ],
      'include_dirs': [
        '<(bindings_v8_dir)',  # FIXME: Remove once http://crbug.com/236119 is fixed.
        'testing',
        'testing/v8',
      ],
      'sources': [
        '<@(webcore_testing_files)',
        '<(bindings_output_dir)/V8GCObservation.cpp',
        '<(bindings_output_dir)/V8GCObservation.h',
        '<(bindings_output_dir)/V8MallocStatistics.cpp',
        '<(bindings_output_dir)/V8MallocStatistics.h',
        '<(bindings_output_dir)/V8TypeConversions.cpp',
        '<(bindings_output_dir)/V8TypeConversions.h',
        '<(bindings_output_dir)/V8Internals.cpp',
        '<(bindings_output_dir)/V8Internals.h',
        '<(bindings_output_dir)/V8InternalProfilers.cpp',
        '<(bindings_output_dir)/V8InternalProfilers.h',
        '<(bindings_output_dir)/V8InternalSettings.cpp',
        '<(bindings_output_dir)/V8InternalSettings.h',
        '<(bindings_output_dir)/V8InternalSettingsGenerated.cpp',
        '<(bindings_output_dir)/V8InternalSettingsGenerated.h',
        '<(bindings_output_dir)/V8InternalRuntimeFlags.cpp',
        '<(bindings_output_dir)/V8InternalRuntimeFlags.h',
        '<(bindings_output_dir)/V8LayerRect.cpp',
        '<(bindings_output_dir)/V8LayerRect.h',
        '<(bindings_output_dir)/V8LayerRectList.cpp',
        '<(bindings_output_dir)/V8LayerRectList.h',
      ],
      'sources/': [
        ['exclude', 'testing/js'],
      ],
    },
  ],  # targets
}
