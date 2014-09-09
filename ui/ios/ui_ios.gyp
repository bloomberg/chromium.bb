# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ui_ios',
      'type': 'static_library',
      # Linkable dependents need to set the linker flag '-ObjC' in order to use
      # the categories in this target (e.g. NSString+CrStringDrawing.h).
      'link_settings': {
        'xcode_settings': {'OTHER_LDFLAGS': ['-ObjC']},
      },
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'NSString+CrStringDrawing.h',
        'NSString+CrStringDrawing.mm',
        'uikit_util.h',
        'uikit_util.mm',
      ],
    },
  ],
}
