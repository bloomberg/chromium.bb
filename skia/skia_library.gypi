# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# This gypi file contains the Skia library.
# In component mode (shared_lib) it is folded into a single shared library with
# the Chrome-specific enhancements but in all other cases it is a separate lib.
{
  'dependencies': [
    'skia_library_opts.gyp:skia_opts',
    '../third_party/zlib/zlib.gyp:zlib',
  ],

  'variables': {
    'variables': {
      'conditions': [
        ['OS== "ios"', {
          'skia_support_gpu': 0,
        }, {
          'skia_support_gpu': 1,
        }],
        ['OS=="ios" or enable_printing == 0', {
          'skia_support_pdf': 0,
        }, {
          'skia_support_pdf': 1,
        }],
      ],
    },
    'skia_support_gpu': '<(skia_support_gpu)',
    'skia_support_pdf': '<(skia_support_pdf)',

    # These two set the paths so we can include skia/gyp/core.gypi
    'skia_src_path': '../third_party/skia/src',
    'skia_include_path': '../third_party/skia/include',

    # This list will contain all defines that also need to be exported to
    # dependent components.
    'skia_export_defines': [
      'SK_ENABLE_INST_COUNT=0',
      'SK_SUPPORT_GPU=<(skia_support_gpu)',
      'GR_GL_CUSTOM_SETUP_HEADER="GrGLConfig_chrome.h"',
    ],

    'default_font_cache_limit': '(20*1024*1024)',

    'conditions': [
      ['OS== "android"', {
        # Android devices are typically more memory constrained, so
        # default to a smaller glyph cache (it may be overriden at runtime
        # when the renderer starts up, depending on the actual device memory).
        'default_font_cache_limit': '(1*1024*1024)',
        'skia_export_defines': [
          'SK_BUILD_FOR_ANDROID',
          'USE_CHROMIUM_SKIA',
        ],
      }],
    ],
  },

  'includes': [
    '../third_party/skia/gyp/core.gypi',
    '../third_party/skia/gyp/effects.gypi',
  ],

  'sources': [
    # this should likely be moved into src/utils in skia
    '../third_party/skia/src/core/SkFlate.cpp',
    # We don't want to add this to Skia's core.gypi since it is
    # Android only. Include it here and remove it for everyone
    # but Android later.
    '../third_party/skia/src/core/SkPaintOptionsAndroid.cpp',

    '../third_party/skia/src/ports/SkImageDecoder_empty.cpp',
    '../third_party/skia/src/images/SkScaledBitmapSampler.cpp',
    '../third_party/skia/src/images/SkScaledBitmapSampler.h',

    '../third_party/skia/src/opts/opts_check_SSE2.cpp',

    '../third_party/skia/src/pdf/SkPDFCatalog.cpp',
    '../third_party/skia/src/pdf/SkPDFCatalog.h',
    '../third_party/skia/src/pdf/SkPDFDevice.cpp',
    '../third_party/skia/src/pdf/SkPDFDocument.cpp',
    '../third_party/skia/src/pdf/SkPDFFont.cpp',
    '../third_party/skia/src/pdf/SkPDFFont.h',
    '../third_party/skia/src/pdf/SkPDFFormXObject.cpp',
    '../third_party/skia/src/pdf/SkPDFFormXObject.h',
    '../third_party/skia/src/pdf/SkPDFGraphicState.cpp',
    '../third_party/skia/src/pdf/SkPDFGraphicState.h',
    '../third_party/skia/src/pdf/SkPDFImage.cpp',
    '../third_party/skia/src/pdf/SkPDFImage.h',
    '../third_party/skia/src/pdf/SkPDFImageStream.cpp',
    '../third_party/skia/src/pdf/SkPDFImageStream.h',
    '../third_party/skia/src/pdf/SkPDFPage.cpp',
    '../third_party/skia/src/pdf/SkPDFPage.h',
    '../third_party/skia/src/pdf/SkPDFResourceDict.cpp',
    '../third_party/skia/src/pdf/SkPDFResourceDict.h',
    '../third_party/skia/src/pdf/SkPDFShader.cpp',
    '../third_party/skia/src/pdf/SkPDFShader.h',
    '../third_party/skia/src/pdf/SkPDFStream.cpp',
    '../third_party/skia/src/pdf/SkPDFStream.h',
    '../third_party/skia/src/pdf/SkPDFTypes.cpp',
    '../third_party/skia/src/pdf/SkPDFTypes.h',
    '../third_party/skia/src/pdf/SkPDFUtils.cpp',
    '../third_party/skia/src/pdf/SkPDFUtils.h',

    '../third_party/skia/src/ports/SkPurgeableMemoryBlock_none.cpp',

    '../third_party/skia/src/ports/SkFontConfigInterface_android.cpp',
    '../third_party/skia/src/ports/SkFontConfigInterface_direct.cpp',

    '../third_party/skia/src/fonts/SkFontMgr_fontconfig.cpp',
    '../third_party/skia/src/ports/SkFontHost_fontconfig.cpp',

    '../third_party/skia/src/ports/SkFontHost_FreeType.cpp',
    '../third_party/skia/src/ports/SkFontHost_FreeType_common.cpp',
    '../third_party/skia/src/ports/SkFontHost_FreeType_common.h',
    '../third_party/skia/src/ports/SkFontConfigParser_android.cpp',
    '../third_party/skia/src/ports/SkFontHost_mac.cpp',
    '../third_party/skia/src/ports/SkFontHost_win.cpp',
    '../third_party/skia/src/ports/SkGlobalInitialization_chromium.cpp',
    '../third_party/skia/src/ports/SkOSFile_posix.cpp',
    '../third_party/skia/src/ports/SkOSFile_stdio.cpp',
    '../third_party/skia/src/ports/SkOSFile_win.cpp',
    '../third_party/skia/src/ports/SkThread_pthread.cpp',
    '../third_party/skia/src/ports/SkThread_win.cpp',
    '../third_party/skia/src/ports/SkTime_Unix.cpp',
    '../third_party/skia/src/ports/SkTLS_pthread.cpp',
    '../third_party/skia/src/ports/SkTLS_win.cpp',

    '../third_party/skia/src/sfnt/SkOTTable_name.cpp',
    '../third_party/skia/src/sfnt/SkOTTable_name.h',
    '../third_party/skia/src/sfnt/SkOTUtils.cpp',
    '../third_party/skia/src/sfnt/SkOTUtils.h',

    '../third_party/skia/include/utils/mac/SkCGUtils.h',
    '../third_party/skia/include/utils/SkDeferredCanvas.h',
    '../third_party/skia/include/utils/SkMatrix44.h',
    '../third_party/skia/src/utils/debugger/SkDebugCanvas.cpp',
    '../third_party/skia/src/utils/debugger/SkDebugCanvas.h',
    '../third_party/skia/src/utils/debugger/SkDrawCommand.cpp',
    '../third_party/skia/src/utils/debugger/SkDrawCommand.h',
    '../third_party/skia/src/utils/debugger/SkObjectParser.cpp',
    '../third_party/skia/src/utils/debugger/SkObjectParser.h',
    '../third_party/skia/src/utils/mac/SkCreateCGImageRef.cpp',
    '../third_party/skia/src/utils/SkBase64.cpp',
    '../third_party/skia/src/utils/SkBase64.h',
    '../third_party/skia/src/utils/SkBitSet.cpp',
    '../third_party/skia/src/utils/SkBitSet.h',
    '../third_party/skia/src/utils/SkDeferredCanvas.cpp',
    '../third_party/skia/src/utils/SkMatrix44.cpp',
    '../third_party/skia/src/utils/SkNullCanvas.cpp',
    '../third_party/skia/include/utils/SkNWayCanvas.h',
    '../third_party/skia/src/utils/SkNWayCanvas.cpp',
    '../third_party/skia/src/utils/SkPictureUtils.cpp',
    '../third_party/skia/src/utils/SkProxyCanvas.cpp',
    '../third_party/skia/src/utils/SkRTConf.cpp',
    '../third_party/skia/include/utils/SkRTConf.h',
    '../third_party/skia/include/pdf/SkPDFDevice.h',
    '../third_party/skia/include/pdf/SkPDFDocument.h',

    '../third_party/skia/include/ports/SkTypeface_win.h',

    '../third_party/skia/include/images/SkImageRef.h',
    '../third_party/skia/include/images/SkImageRef_GlobalPool.h',
    '../third_party/skia/include/images/SkMovie.h',
    '../third_party/skia/include/images/SkPageFlipper.h',

    '../third_party/skia/include/utils/SkNullCanvas.h',
    '../third_party/skia/include/utils/SkPictureUtils.h',
    '../third_party/skia/include/utils/SkProxyCanvas.h',
  ],
  'include_dirs': [
    '..',
    'config',
    '../third_party/skia/include/config',
    '../third_party/skia/include/core',
    '../third_party/skia/include/effects',
    '../third_party/skia/include/images',
    '../third_party/skia/include/lazy',
    '../third_party/skia/include/pathops',
    '../third_party/skia/include/pdf',
    '../third_party/skia/include/pipe',
    '../third_party/skia/include/ports',
    '../third_party/skia/include/utils',
    '../third_party/skia/src/core',
    '../third_party/skia/src/image',
    '../third_party/skia/src/ports',
    '../third_party/skia/src/sfnt',
    '../third_party/skia/src/utils',
    '../third_party/skia/src/lazy',
  ],
  'conditions': [
    ['skia_support_gpu != 0', {
      'includes': [
        '../third_party/skia/gyp/gpu.gypi',
      ],
      'sources': [
        '<@(skgpu_sources)',
      ],
      'include_dirs': [
        '../third_party/skia/include/gpu',
        '../third_party/skia/src/gpu',
      ],
    }],
    ['skia_support_pdf == 0', {
      'sources/': [
        ['exclude', '../third_party/skia/src/pdf/']
      ],
    }],
    ['skia_support_pdf == 1', {
      'dependencies': [
        '../third_party/sfntly/sfntly.gyp:sfntly',
      ],
    }],

    #Settings for text blitting, chosen to approximate the system browser.
    [ 'OS == "linux"', {
      'defines': [
        'SK_GAMMA_EXPONENT=1.2',
        'SK_GAMMA_CONTRAST=0.2',
      ],
    }],
    ['OS == "android"', {
      'defines': [
        'SK_GAMMA_APPLY_TO_A8',
        'SK_GAMMA_EXPONENT=1.4',
        'SK_GAMMA_CONTRAST=0.0',
      ],
    }],
    ['OS == "win"', {
      'defines': [
        'SK_GAMMA_SRGB',
        'SK_GAMMA_CONTRAST=0.5',
      ],
    }],
    ['OS == "mac"', {
      'defines': [
        'SK_GAMMA_SRGB',
        'SK_GAMMA_CONTRAST=0.0',
      ],
    }],

    # For POSIX platforms, prefer the Mutex implementation provided by Skia
    # since it does not generate static initializers.
    [ 'OS == "android" or OS == "linux" or OS == "mac" or OS == "ios"', {
      'defines+': [
        'SK_USE_POSIX_THREADS',
      ],
      'direct_dependent_settings': {
        'defines': [
          'SK_USE_POSIX_THREADS',
        ],
      },
    }],

    [ 'OS != "android"', {
      'sources!': [
        '../third_party/skia/src/core/SkPaintOptionsAndroid.cpp',
      ],
    }],
    [ 'OS != "ios"', {
      'dependencies': [
        '../third_party/WebKit/public/blink_skia_config.gyp:blink_skia_config',
      ],
      'export_dependent_settings': [
        '../third_party/WebKit/public/blink_skia_config.gyp:blink_skia_config',
      ],
    }],
    [ 'OS != "mac"', {
      'sources/': [
        ['exclude', '/mac/']
      ],
    }],
    [ 'target_arch == "arm" and arm_version >= 7 and arm_neon == 1', {
      'defines': [
        '__ARM_HAVE_NEON',
      ],
    }],
    [ 'target_arch == "arm" and arm_version >= 7 and arm_neon_optional == 1', {
      'defines': [
        '__ARM_HAVE_OPTIONAL_NEON_SUPPORT',
      ],
    }],
    [ 'OS == "android" and target_arch == "arm"', {
      'sources': [
        '../third_party/skia/src/core/SkUtilsArm.cpp',
      ],
      'includes': [
        '../build/android/cpufeatures.gypi',
      ],
    }],
    [ 'target_arch == "arm" or target_arch == "mipsel"', {
      'sources!': [
        '../third_party/skia/src/opts/opts_check_SSE2.cpp'
      ],
    }],
    [ 'use_glib == 1', {
      'dependencies': [
        '../build/linux/system.gyp:fontconfig',
        '../build/linux/system.gyp:freetype2',
        '../build/linux/system.gyp:pangocairo',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'cflags': [
        '-Wno-unused',
        '-Wno-unused-function',
      ],
    }],
    [ 'use_glib == 0', {
      'sources!': [
        '../third_party/skia/src/ports/SkFontConfigInterface_direct.cpp',
        '../third_party/skia/src/fonts/SkFontMgr_fontconfig.cpp',
      ],
    }],
    [ 'use_glib == 0 and OS != "android"', {
      'sources!': [
        '../third_party/skia/src/ports/SkFontHost_FreeType.cpp',
        '../third_party/skia/src/ports/SkFontHost_FreeType_common.cpp',
        '../third_party/skia/src/ports/SkFontHost_fontconfig.cpp',

      ],
    }],
    [ 'OS == "android"', {
      'dependencies': [
        '../third_party/expat/expat.gyp:expat',
        '../third_party/freetype/freetype.gyp:ft2',
      ],
      # This exports a hard dependency because it needs to run its
      # symlink action in order to expose the skia header files.
      'hard_dependency': 1,
      'include_dirs': [
        '../third_party/expat/files/lib',
      ],
    }],
    [ 'OS == "ios"', {
      'defines': [
        'SK_BUILD_FOR_IOS',
        'SK_USE_MAC_CORE_TEXT',
      ],
      'include_dirs': [
        '../third_party/skia/include/utils/ios',
        '../third_party/skia/include/utils/mac',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/ImageIO.framework',
        ],
      },
      'sources': [
        # This file is used on both iOS and Mac, so it should be removed
        #  from the ios and mac conditions and moved into the main sources
        #  list.
        '../third_party/skia/src/utils/mac/SkStream_mac.cpp',
      ],
      'sources/': [
        ['exclude', 'opts_check_SSE2\\.cpp$'],
      ],

      # The main skia_opts target does not currently work on iOS because the
      # target architecture on iOS is determined at compile time rather than
      # gyp time (simulator builds are x86, device builds are arm).  As a
      # temporary measure, this is a separate opts target for iOS-only, using
      # the _none.cpp files to avoid architecture-dependent implementations.
      'dependencies': [
        'skia_library_opts.gyp:skia_opts_none',
      ],
      'dependencies!': [
        'skia_library_opts.gyp:skia_opts',
      ],
    }],
    [ 'OS == "mac"', {
      'defines': [
        'SK_BUILD_FOR_MAC',
        'SK_USE_MAC_CORE_TEXT',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../third_party/skia/include/utils/mac',
        ],
      },
      'include_dirs': [
        '../third_party/skia/include/utils/mac',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
        ],
      },
      'sources': [
        '../third_party/skia/src/utils/mac/SkStream_mac.cpp',
      ],
    }],
    [ 'OS == "win"', {
      'sources!': [
        '../third_party/skia/src/ports/SkOSFile_posix.cpp',
        '../third_party/skia/src/ports/SkThread_pthread.cpp',
        '../third_party/skia/src/ports/SkTime_Unix.cpp',
        '../third_party/skia/src/ports/SkTLS_pthread.cpp',
      ],
    }],
    # TODO(scottmg): http://crbug.com/177306
    ['clang==1', {
      'xcode_settings': {
        'WARNING_CFLAGS!': [
          # Don't warn about string->bool used in asserts.
          '-Wstring-conversion',
        ],
      },
      'cflags!': [
        '-Wstring-conversion',
      ],
    }],
  ],
  'target_conditions': [
    # Pull in specific Mac files for iOS (which have been filtered out
    # by file name rules).
    [ 'OS == "ios"', {
      'sources/': [
        ['include', 'SkFontHost_mac\\.cpp$',],
        ['include', 'SkStream_mac\\.cpp$',],
        ['include', 'SkCreateCGImageRef\\.cpp$',],
      ],
    }],
  ],

  'defines': [
    '<@(skia_export_defines)',

    # this flag can be removed entirely once this has baked for a while
    'SK_ALLOW_OVER_32K_BITMAPS',

    # skia uses static initializers to initialize the serialization logic
    # of its "pictures" library. This is currently not used in chrome; if
    # it ever gets used the processes that use it need to call
    # SkGraphics::Init().
    'SK_ALLOW_STATIC_GLOBAL_INITIALIZERS=0',

    # Disable this check because it is too strict for some Chromium-specific
    # subclasses of SkPixelRef. See bug: crbug.com/171776.
    'SK_DISABLE_PIXELREF_LOCKCOUNT_BALANCE_CHECK',

    'IGNORE_ROT_AA_RECT_OPT',

    'SKIA_IGNORE_GPU_MIPMAPS',

    'SK_GDI_ALWAYS_USE_TEXTMETRICS_FOR_FONT_METRICS',

    'SK_DEFAULT_FONT_CACHE_LIMIT=<(default_font_cache_limit)',
  ],

  'direct_dependent_settings': {
    'include_dirs': [
      #temporary until we can hide SkFontHost
      '../third_party/skia/src/core',

      'config',
      '../third_party/skia/include/config',
      '../third_party/skia/include/core',
      '../third_party/skia/include/effects',
      '../third_party/skia/include/pdf',
      '../third_party/skia/include/gpu',
      '../third_party/skia/include/lazy',
      '../third_party/skia/include/pathops',
      '../third_party/skia/include/pipe',
      '../third_party/skia/include/ports',
      '../third_party/skia/include/utils',
    ],
    'defines': [
      '<@(skia_export_defines)',
    ],
  },
}
