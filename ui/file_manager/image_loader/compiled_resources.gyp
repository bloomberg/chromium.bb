# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'background',
      'variables': {
        'depends': [
          'image_loader.js',
          'cache.js',
          'scheduler.js',
          'request.js',
        ],
        'externs': [
          '<(CLOSURE_DIR)/externs/chrome_send_externs.js',
          '<(CLOSURE_DIR)/externs/chrome_extensions.js',
          '<(CLOSURE_DIR)/externs/file_manager_private.js',
        ],
      },
      'includes': [
        '../../../third_party/closure_compiler/compile_js.gypi'
      ],
    }
  ],
}


