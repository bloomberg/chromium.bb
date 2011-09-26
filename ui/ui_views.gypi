# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'v2',
      'type': 'static_library',
      'dependencies': [
        '../skia/skia.gyp:skia',
        '../ui/base/strings/ui_strings.gyp:ui_strings',
        'ui',
      ],
      'sources': [
        'views/events/accelerator.cc',
        'views/events/accelerator.h',
        'views/events/context_menu_controller.h',
        'views/events/drag_controller.h',
        'views/events/event.cc',
        'views/events/event.h',
        'views/events/focus_event.cc',
        'views/events/focus_event.h',
        'views/focus/accelerator_handler.h',
        'views/focus/accelerator_handler_win.cc',
        'views/focus/focus_manager.cc',
        'views/focus/focus_manager.h',
        'views/focus/focus_search.cc',
        'views/focus/focus_search.h',
        'views/layout/fill_layout.cc',
        'views/layout/fill_layout.h',
        'views/layout/layout_manager.cc',
        'views/layout/layout_manager.h',
        'views/rendering/border.cc',
        'views/rendering/border.h',
        'views/view.cc',
        'views/view.h',
        'views/widget/native_widget.h',
        'views/widget/native_widget_listener.h',
        'views/widget/native_widget_views.cc',
        'views/widget/native_widget_views.h',
        'views/widget/native_widget_win.cc',
        'views/widget/native_widget_win.h',
        'views/widget/root_view.cc',
        'views/widget/root_view.h',
        'views/widget/widget.cc',
        'views/widget/widget.h',
        'views/window/window.cc',
        'views/window/window.h',
        'views/window/native_window.h',
        'views/window/native_window_views.cc',
        'views/window/native_window_views.h',
        'views/window/native_window_win.cc',
        'views/window/native_window_win.h',
      ],
      'include_dirs': [
        '../',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../build/linux/system.gyp:x11',
            '../build/linux/system.gyp:xext',
          ],
          'sources!': [
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            '../third_party/wtl/include',
          ],
        }],
      ],
    },
    {
      'target_name': 'views_demo',
      'type': 'executable',
      'dependencies': [
        '../skia/skia.gyp:skia',
        'v2',
      ],
      'sources': [
        'views/demo/main.cc',
      ],
      'include_dirs': [
        '../',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'views/widget/widget.rc',
            'views/widget/widget_resource.h',
          ],
        }],
      ],
    },
  ],
}
