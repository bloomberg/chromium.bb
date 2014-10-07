# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'main_scripts',
      'variables': {
        'depends': [
          '../../../../../third_party/jstemplate/compiled_resources.gyp:jstemplate',
          '../../../../../ui/webui/resources/js/cr/ui/dialogs.js',
          '../../common/js/util.js',
          '../../common/js/async_util.js',
          '../../common/js/volume_manager_common.js',
          '../../background/js/file_operation_manager.js',
          '../../background/js/volume_manager.js',
          '../../background/js/progress_center.js',
        ],
        'externs': [
          '<(CLOSURE_DIR)/externs/chrome_send_externs.js',
          '<(CLOSURE_DIR)/externs/chrome_extensions.js',
          '<(CLOSURE_DIR)/externs/file_manager_private.js',
          '<(CLOSURE_DIR)/externs/metrics_private.js',
        ],
      },
      'includes': [
        '../../../../../third_party/closure_compiler/compile_js.gypi'
      ],
    }
  ],
}
