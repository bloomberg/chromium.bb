# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'web_dialogs',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../content/content.gyp:content_browser',
        '../../skia/skia.gyp:skia',
      ],
      'defines': [
        'WEB_DIALOGS_IMPLEMENTATION',
      ],
      'sources': [
        # All .cc, .h under web_dialogs, except unittests.
        'constrained_web_dialog_ui.cc',
        'constrained_web_dialog_ui.h',
        'web_dialog_delegate.cc',
        'web_dialog_delegate.h',
        'web_dialog_observer.h',
        'web_dialog_ui.cc',
        'web_dialog_ui.h',
        'web_dialogs_export.h',
      ],
    },
  ],
}
