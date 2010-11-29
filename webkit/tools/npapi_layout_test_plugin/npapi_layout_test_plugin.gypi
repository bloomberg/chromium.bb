# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'npapi_layout_test_plugin',
      'type': 'loadable_module',
      'variables': {
        'chromium_code': 1,
      },
      'mac_bundle': 1,
      'msvs_guid': 'BE6D5659-A8D5-4890-A42C-090DD10EF62C',
      'sources': [
        'PluginObject.cpp',
        'TestObject.cpp',
        'main.cpp',
        'npapi_layout_test_plugin.def',
        'npapi_layout_test_plugin.rc',
      ],
      'include_dirs': [
        '../../..',
      ],
      'dependencies': [
        '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
      ],
      'msvs_disabled_warnings': [ 4996 ],
      'mac_bundle_resources': [
        'Info.r',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': '<(DEPTH)/webkit/tools/npapi_layout_test_plugin/Info.plist',
      },
      'conditions': [
        ['inside_chromium_build==0', {
          'dependencies': ['../../../../JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:wtf'],
        },{
          'dependencies': ['<(DEPTH)/third_party/WebKit/JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:wtf'],
        }],
        ['OS!="win"', {
          'sources!': [
            'npapi_layout_test_plugin.def',
            'npapi_layout_test_plugin.rc',
          ],
        }, { # OS == "win"
          'variables': {
            # This is not a relative pathname.  Avoid pathname relativization
            # by sticking it in a variable that isn't recognized as one
            # containing pathnames, and by using the >(late) form of variable
            # expansion.
            'winmm_lib': 'winmm.lib',
          },
          'link_settings': {
            'libraries': [
              '>(winmm_lib)',
            ],
          },
        }],
        ['OS=="mac"', {
          'product_name': 'TestNetscapePlugIn',
          'product_extension': 'plugin',
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
            ],
          },
        }],
        ['(OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris") and (target_arch=="x64" or target_arch=="arm")', {
          # Shared libraries need -fPIC on x86-64
          'cflags': ['-fPIC']
        }],
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
