# Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
      'target_name': 'v2',
      'type': '<(library)',
      'msvs_guid': '70760ECA-4D8B-47A4-ACDC-D3E7F25F0543',
      'dependencies': [
        '<(DEPTH)/app/app.gyp:app_base',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/ui/base/strings/ui_strings.gyp:ui_strings',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
      ],
      'sources': [
        'events/accelerator.cc',
        'events/accelerator.h',
        'events/context_menu_controller.h',
        'events/drag_controller.h',
        'events/event.cc',
        'events/event.h',
        'events/event_win.cc',
        'events/focus_event.cc',
        'events/focus_event.h',
        'focus/accelerator_handler.h',
        'focus/accelerator_handler_win.cc',
        'focus/focus_manager.cc',
        'focus/focus_manager.h',
        'focus/focus_search.cc',
        'focus/focus_search.h',
        'layout/fill_layout.cc',
        'layout/fill_layout.h',
        'layout/layout_manager.cc',
        'layout/layout_manager.h',
        'native_types.h',
        'rendering/border.cc',
        'rendering/border.h',
        'view.cc',
        'view.h',
        'widget/native_widget.h',
        'widget/native_widget_listener.h',
        'widget/native_widget_views.cc',
        'widget/native_widget_views.h',
        'widget/native_widget_win.cc',
        'widget/native_widget_win.h',
        'widget/root_view.cc',
        'widget/root_view.h',
        'widget/widget.cc',
        'widget/widget.h',
        'window/window.cc',
        'window/window.h',
        'window/native_window.h',
        'window/native_window_views.cc',
        'window/native_window_views.h',
        'window/native_window_win.cc',
        'window/native_window_win.h',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'conditions': [
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:gtk',
            '<(DEPTH)/build/linux/system.gyp:x11',
            '<(DEPTH)/build/linux/system.gyp:xext',
          ],
          'sources!': [
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
        }],
      ],
    },
    {
      'target_name': 'views_unittests',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        'v2',
      ],
      'sources': [
        'rendering/border_unittest.cc',
        'run_all_unittests.cc',
        'view_unittest.cc',
        'widget/native_widget_win_unittest.cc',
        'widget/root_view_unittest.cc',
        'widget/widget_test_util.cc',
        'widget/widget_test_util.h',
        'widget/widget_unittest.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'conditions': [
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:gtk',
            '<(DEPTH)/chrome/chrome.gyp:packed_resources',
          ],
          'conditions': [
            ['linux_use_tcmalloc==1', {
               'dependencies': [
                 '<(DEPTH)/base/allocator/allocator.gyp:allocator',
               ],
            }],
          ],
        },
        ],
        ['OS=="win"', {
          'sources': [
            'widget/widget.rc',
            'widget/widget_resource.h',
          ],
          'link_settings': {
            'libraries': [
              '-limm32.lib',
              '-loleacc.lib',
            ]
          },
        }],
      ],
    },
    {
      'target_name': 'views_demo',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/skia/skia.gyp:skia',
        'v2',
      ],
      'sources': [
        'demo/main.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'widget/widget.rc',
            'widget/widget_resource.h',
          ],
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
