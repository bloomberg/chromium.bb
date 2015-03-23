# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'background',
      'variables': {
        'depends': [
          "../file_manager/foreground/js/metadata/image_orientation.js",
          'cache.js',
          'image_loader.js',
          'piex_loader.js',
          'request.js',
          'scheduler.js',
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
