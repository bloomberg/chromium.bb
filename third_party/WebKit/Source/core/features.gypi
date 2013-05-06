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
  # The following defines turn WebKit features on and off.
  'variables': {
    'feature_defines': [
      'ENABLE_3D_PLUGIN=1',
      'ENABLE_BATTERY_STATUS=0',
      'ENABLE_CANVAS_USES_MAILBOX=0',
      'ENABLE_CSS3_TEXT=0',
      'ENABLE_CSS_COMPOSITING=0',
      'ENABLE_CSS_DEVICE_ADAPTATION=0',
      'ENABLE_CSS_EXCLUSIONS=1',
      'ENABLE_CSS_REGIONS=1',
      'ENABLE_CUSTOM_SCHEME_HANDLER=0',
      'ENABLE_ENCRYPTED_MEDIA=1',
      'ENABLE_JAVASCRIPT_I18N_API=1',
      'ENABLE_RESOLUTION_MEDIA_QUERY=0',
      'ENABLE_RUBY=1',
      'ENABLE_SANDBOX=1',
      'ENABLE_SVG=<(enable_svg)',
      'ENABLE_SVG_FONTS=<(enable_svg)',
      'ENABLE_TOUCH_ICON_LOADING=<(enable_touch_icon_loading)',
      'ENABLE_WEBGL=1',
      'ENABLE_XHR_TIMEOUT=0',
      'WTF_USE_BUILTIN_UTF8_CODEC=1',
      # WTF_USE_DYNAMIC_ANNOTATIONS=1 may be defined in build/common.gypi
      # We can't define it here because it should be present only
      # in Debug or release_valgrind_build=1 builds.
      'WTF_USE_WEBKIT_IMAGE_DECODERS=1',
    ],
    # We have to nest variables inside variables so that they can be overridden
    # through GYP_DEFINES.
    'variables': {
      'enable_svg%': 1,
      'enable_touch_icon_loading%' : 0,
    },
    'enable_svg%': '<(enable_svg)',
    'conditions': [
      ['use_concatenated_impulse_responses==1', {
        # Use concatenated HRTF impulse responses
        'feature_defines': ['WTF_USE_CONCATENATED_IMPULSE_RESPONSES=1'],
      }],
      ['OS=="android"', {
        'feature_defines': [
          'ENABLE_CALENDAR_PICKER=0',
          'ENABLE_DATALIST_ELEMENT=0',
          'ENABLE_FAST_MOBILE_SCROLLING=1',
          'ENABLE_INPUT_SPEECH=0',
          'ENABLE_INPUT_TYPE_COLOR=0',
          'ENABLE_LEGACY_NOTIFICATIONS=0',
          'ENABLE_MEDIA_CAPTURE=1',
          'ENABLE_NOTIFICATIONS=0',
          'ENABLE_ORIENTATION_EVENTS=1',
          'ENABLE_PAGE_POPUP=0',
          'ENABLE_PRINTING=0',
          'ENABLE_NAVIGATOR_CONTENT_UTILS=0',
          'WTF_USE_NATIVE_FULLSCREEN_VIDEO=1',
        ],
        'enable_touch_icon_loading': 1,
      }, { # OS!="android"
        'feature_defines': [
          'ENABLE_CALENDAR_PICKER=1',
          'ENABLE_DATALIST_ELEMENT=1',
          'ENABLE_INPUT_SPEECH=1',
          'ENABLE_INPUT_TYPE_COLOR=1',
          'ENABLE_INPUT_MULTIPLE_FIELDS_UI=1',
          'ENABLE_LEGACY_NOTIFICATIONS=1',
          'ENABLE_MEDIA_CAPTURE=0',
          'ENABLE_NAVIGATOR_CONTENT_UTILS=1',
          'ENABLE_NOTIFICATIONS=1',
          'ENABLE_ORIENTATION_EVENTS=0',
          'ENABLE_PAGE_POPUP=1',
          'ENABLE_PRINTING=1',
          'ENABLE_WEB_AUDIO=1',
        ],
      }],
      ['OS=="linux" or OS=="mac" or OS=="android"', {
        'feature_defines': [
          # 8Bit text runs should be enabled for all platforms webkit.org/b/111348
          'ENABLE_8BIT_TEXTRUN=1',
          'ENABLE_BINDING_INTEGRITY=1',
        ],
      }, { # OS!="linux"
        'feature_defines': [
          'ENABLE_BINDING_INTEGRITY=0',
        ],
      }],
      # Mac OS X uses Accelerate.framework FFT by default instead of FFmpeg.
      ['OS!="mac" and OS!="android"', {
        'feature_defines': [
          'WTF_USE_WEBAUDIO_FFMPEG=1',
        ],
      }],
      ['OS=="android" and use_openmax_dl_fft!=0', {
        'feature_defines': [
          'WTF_USE_WEBAUDIO_OPENMAX_DL_FFT=1',
          # Enabling the FFT is enough to enable WebAudio support to
          # allow most WebAudio features to work on Android.
          'ENABLE_WEB_AUDIO=1',
        ],
      }],
      ['OS=="win" or OS=="android" or OS=="linux"', {
        'feature_defines': [
          'ENABLE_OPENTYPE_VERTICAL=1',
        ],
      }],
      ['OS=="win"', {
        'feature_defines': [
          'ENABLE_PAN_SCROLLING=1',
        ],
      }],
      ['OS=="mac"', {
        'feature_defines': [
          'ENABLE_RUBBER_BANDING=1',
        ],
      }],
      ['OS=="linux" or OS=="android"', {
        'feature_defines': [
          'WTF_USE_HARFBUZZ=1',
        ],
      }],
      ['use_default_render_theme==1', {
        'feature_defines': [
          'ENABLE_DEFAULT_RENDER_THEME=1',
        ],
      }],
    ],
  },
}
