# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'feature_defines': [
      'ENABLE_CHANNEL_MESSAGING=1',
      'ENABLE_DATABASE=1',
      'ENABLE_DATAGRID=0',
      'ENABLE_OFFLINE_WEB_APPLICATIONS=1',
      'ENABLE_DASHBOARD_SUPPORT=0',
      'ENABLE_DOM_STORAGE=1',
      'ENABLE_JAVASCRIPT_DEBUGGER=0',
      'ENABLE_JSC_MULTIPLE_THREADS=0',
      'ENABLE_ICONDATABASE=0',
      'ENABLE_NOTIFICATIONS=0',
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
      'WEBCORE_NAVIGATOR_VENDOR="Google Inc."', 
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

          # Chromium's version of WebCore includes the following Objective-C
          # classes. The system-provided WebCore framework may also provide
          # these classes. Because of the nature of Objective-C binding
          # (dynamically at runtime), it's possible for the Chromium-provided
          # versions to interfere with the system-provided versions.  This may
          # happen when a system framework attempts to use WebCore.framework,
          # such as when converting an HTML-flavored string to an
          # NSAttributedString.  The solution is to force Objective-C class
          # names that would conflict to use alternate names.

          # TODO(mark) This list will hopefully shrink but may also grow.
          # Periodically run:
          # nm libwebcore.a | grep -E '[atsATS] ([+-]\[|\.objc_class_name)'
          # and make sure that everything listed there has the alternate
          # ChromiumWebCoreObjC name, and that nothing extraneous is listed
          # here. If all Objective-C can be eliminated from Chromium's WebCore
          # library, these defines should be removed entirely.
          # TODO(yaar) move these out of command line defines.
          'ScrollbarPrefsObserver=ChromiumWebCoreObjCScrollbarPrefsObserver',
          'WebCoreRenderThemeNotificationObserver=ChromiumWebCoreObjCWebCoreRenderThemeNotificationObserver',
          'WebFontCache=ChromiumWebCoreObjCWebFontCache',
        ],
      }],
      ['OS=="win"', {
        'non_feature_defines': [
          'CRASH=__debugbreak',
          # Match Safari and Mozilla on Windows.
          'WEBCORE_NAVIGATOR_PLATFORM="Win32"',
        ],
      }],
    ],
  }, # variables
}
