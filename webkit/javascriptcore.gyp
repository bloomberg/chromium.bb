# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../third_party/WebKit/JavaScriptCore/JavaScriptCore.gypi',
  ],
  'targets': [
    {
      'target_name': 'wtf',
      'type': '<(library)',
      'msvs_guid': 'AA8A5A85-592B-4357-BC60-E0E91E026AF6',
      'dependencies': [
        'config.gyp:config',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'include_dirs': [
        '../third_party/WebKit/JavaScriptCore',
        '../third_party/WebKit/JavaScriptCore/wtf',
        '../third_party/WebKit/JavaScriptCore/wtf/unicode',
      ],
      'sources': [
        '<@(javascriptcore_files)',
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
        'config.gyp:config',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
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
        }],
        ['OS=="linux" or OS=="freebsd"', {
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
        'config.gyp:config',
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
  ], # targets
}
