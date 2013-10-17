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
    '../build/win/precompile.gypi',
    '../build/features.gypi',
    'blink_platform.gypi',
  ],
  'targets': [{
    'target_name': 'blink_common',
    'type': '<(component)',
    'variables': { 'enable_wexit_time_destructors': 1 },
    'dependencies': [
      '../config.gyp:config',
      '../wtf/wtf.gyp:wtf',
      # FIXME: Can we remove the dependencies on V8 and Skia?
      '<(DEPTH)/skia/skia.gyp:skia',
      '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
    ],
    'export_dependent_settings': [
      '<(DEPTH)/skia/skia.gyp:skia',
      '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
    ],
    'defines': [
      'BLINK_COMMON_IMPLEMENTATION=1',
      'INSIDE_BLINK',
    ],
    'sources': [
      'exported/WebCString.cpp',
      'exported/WebString.cpp',
      'exported/WebCommon.cpp',
    ],
  }, {
    'target_name': 'blink_platform',
    'type': '<(component)',
    'dependencies': [
      '../config.gyp:config',
      '../wtf/wtf.gyp:wtf',
      '../weborigin/weborigin.gyp:weborigin',
      '<(DEPTH)/skia/skia.gyp:skia',
      # FIXME: This dependency exists for CSS Custom Filters, via the file ANGLEPlatformBridge
      # The code touching ANGLE should really be moved into the ANGLE directory.
      '<(DEPTH)/third_party/angle_dx11/src/build_angle.gyp:translator',
      '<(DEPTH)/url/url.gyp:url_lib',
      '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
      '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
      'platform_derived_sources.gyp:make_derived_sources',
      'blink_common',
    ],
    'export_dependent_settings': [
      # FIXME: This dependency exists for CSS Custom Filters, via the file ANGLEPlatformBridge
      # The code touching ANGLE should really be moved into the ANGLE directory.
      '<(DEPTH)/third_party/angle_dx11/src/build_angle.gyp:translator',
    ],
    'defines': [
      'BLINK_PLATFORM_IMPLEMENTATION=1',
      'INSIDE_BLINK',
    ],
    'include_dirs': [
      '<(DEPTH)/third_party/angle_dx11/include',
    ],
    'sources': [
      '<@(platform_files)',

      # Additional .cpp files from platform_derived_sources.gyp:make_derived_sources actions.
      '<(SHARED_INTERMEDIATE_DIR)/blink/FontFamilyNames.cpp',
    ],
    # Disable c4267 warnings until we fix size_t to int truncations.
    # Disable c4724 warnings which is generated in VS2012 due to improper
    # compiler optimizations, see crbug.com/237063
    'msvs_disabled_warnings': [ 4267, 4334, 4724 ],
    'conditions': [
      ['OS=="mac"', {
        'link_settings': {
          'libraries': [
            '$(SDKROOT)/System/Library/Frameworks/Accelerate.framework',
            '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
            '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
          ]
        },
        'sources/': [
          # We use LocaleMac.mm instead of LocaleICU.cpp
          ['exclude', 'LocaleICU\\.(cpp|h)$'],
        ],
      }, { # OS!="mac"
        'sources/': [
          ['exclude', 'mac/'],
        ],
      }],
      ['OS=="win"', {
        'sources/': [
          # We use LocaleWin.cpp instead of LocaleICU.cpp
          ['exclude', 'LocaleICU\\.(cpp|h)$'],
        ],
      }, { # OS!="win"
        'sources/': [
          ['exclude', 'win/'],
        ],
      }],
      ['"WTF_USE_WEBAUDIO_FFMPEG=1" in feature_defines', {
        'include_dirs': [
          '<(DEPTH)/third_party/ffmpeg',
        ],
        'dependencies': [
          '<(DEPTH)/third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
        ],
      }],
      ['"WTF_USE_WEBAUDIO_OPENMAX_DL_FFT=1" in feature_defines', {
         'include_dirs': [
           '<(DEPTH)/third_party/openmax_dl',
         ],
        'dependencies': [
          '<(DEPTH)/third_party/openmax_dl/dl/dl.gyp:openmax_dl',
        ],
      }],
    ],
  }],
}
