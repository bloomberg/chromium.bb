# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'ui_base',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        'ui_gfx',
      ],
      # Export these dependencies since text_elider.h includes ICU headers.
      'export_dependent_settings': [
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'sources': [
        'base/accessibility/accessibility_types.h',
        'base/accessibility/accessible_view_state.h',
        'base/accessibility/accessible_view_state.cc',
        'base/animation/animation.cc',
        'base/animation/animation.h',
        'base/animation/animation_container.cc',
        'base/animation/animation_container.h',
        'base/animation/animation_container_element.h',
        'base/animation/animation_container_observer.h',
        'base/animation/animation_delegate.h',
        'base/animation/linear_animation.cc',
        'base/animation/linear_animation.h',
        'base/animation/multi_animation.cc',
        'base/animation/multi_animation.h',
        'base/animation/slide_animation.cc',
        'base/animation/slide_animation.h',
        'base/animation/throb_animation.cc',
        'base/animation/throb_animation.h',
        'base/animation/tween.cc',
        'base/animation/tween.h',
        'base/clipboard/clipboard.cc',
        'base/clipboard/clipboard.h',
        'base/clipboard/clipboard_linux.cc',
        'base/clipboard/clipboard_mac.mm',
        'base/clipboard/clipboard_util_win.cc',
        'base/clipboard/clipboard_util_win.h',
        'base/clipboard/clipboard_win.cc',
        'base/clipboard/scoped_clipboard_writer.cc',
        'base/clipboard/scoped_clipboard_writer.h',
        'base/ime/composition_text.cc',
        'base/ime/composition_text.h',
        'base/ime/composition_underline.h',
        'base/ime/text_input_type.h',
        'base/gtk/gtk_im_context_util.cc',
        'base/gtk/gtk_im_context_util.h',
        'base/range/range.cc',
        'base/range/range.h',
        'base/range/range.mm',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../build/linux/system.gyp:x11',
            '../build/linux/system.gyp:xext',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'base/win/ime_input.cc',
            'base/win/ime_input.h',
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
