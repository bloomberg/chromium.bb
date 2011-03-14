# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'target_defaults': {
    'sources/': [
      ['exclude', '/(cocoa|gtk|win)/'],
      ['exclude', '_(cocoa|gtk|linux|mac|posix|skia|win|x)\\.(cc|mm?)$'],
      ['exclude', '/(gtk|win|x11)_[^/]*\\.cc$'],
    ],
    'conditions': [
      ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {'sources/': [
        ['include', '/gtk/'],
        ['include', '_(gtk|linux|posix|skia|x)\\.cc$'],
        ['include', '/(gtk|x11)_[^/]*\\.cc$'],
      ]}],
      ['OS=="mac"', {'sources/': [
        ['include', '/cocoa/'],
        ['include', '_(cocoa|mac|posix)\\.(cc|mm?)$'],
      ]}, { # else: OS != "mac"
        'sources/': [
          ['exclude', '\\.mm?$'],
        ],
      }],
      ['OS=="win"', {'sources/': [
        ['include', '_(win)\\.cc$'],
        ['include', '/win/'],
        ['include', '/win_[^/]*\\.cc$'],
      ]}],
      ['touchui==0', {'sources/': [
        ['exclude', 'event_x.cc$'],
        ['exclude', 'native_menu_x.cc$'],
        ['exclude', 'native_menu_x.h$'],
        ['exclude', 'touchui/'],
        ['exclude', '_(touch)\\.cc$'],
      ]}],
    ],
  },
  'targets': [
    {
      'target_name': 'ui_base',
      'type': '<(library)',
      'dependencies': [
        '../base/base.gyp:base',
        '../ui/gfx/gfx.gyp:gfx',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      # Export these dependencies since text_elider.h includes ICU headers.
      'export_dependent_settings': [
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'sources': [
        'accessibility/accessibility_types.h',
        'accessibility/accessible_view_state.h',
        'accessibility/accessible_view_state.cc',
        'animation/animation.cc',
        'animation/animation.h',
        'animation/animation_container.cc',
        'animation/animation_container.h',
        'animation/animation_container_element.h',
        'animation/animation_container_observer.h',
        'animation/animation_delegate.h',
        'animation/linear_animation.cc',
        'animation/linear_animation.h',
        'animation/multi_animation.cc',
        'animation/multi_animation.h',
        'animation/slide_animation.cc',
        'animation/slide_animation.h',
        'animation/throb_animation.cc',
        'animation/throb_animation.h',
        'animation/tween.cc',
        'animation/tween.h',
        'clipboard/clipboard.cc',
        'clipboard/clipboard.h',
        'clipboard/clipboard_linux.cc',
        'clipboard/clipboard_mac.mm',
        'clipboard/clipboard_util_win.cc',
        'clipboard/clipboard_util_win.h',
        'clipboard/clipboard_win.cc',
        'clipboard/scoped_clipboard_writer.cc',
        'clipboard/scoped_clipboard_writer.h',
      ],
      'conditions': [
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../build/linux/system.gyp:x11',
            '../build/linux/system.gyp:xext',
          ],
        }],
      ],
    },
    {
      'target_name': 'ui_base_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'ui_base',
      ],
      'sources': [
        'animation/animation_container_unittest.cc',
        'animation/animation_unittest.cc',
        'animation/multi_animation_unittest.cc',
        'animation/slide_animation_unittest.cc',
        'clipboard/clipboard_unittest.cc',
        'run_all_unittests.cc',
        'test_suite.h',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
