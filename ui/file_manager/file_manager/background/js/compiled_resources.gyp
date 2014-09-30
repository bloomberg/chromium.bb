# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'background',
      'variables': {
        'depends': [
          '../../common/js/error_util.js',
          '../../../../webui/resources/js/load_time_data.js',
          '../../../../webui/resources/js/cr.js',
          '../../../../webui/resources/js/cr/event_target.js',
          '../../../../webui/resources/js/cr/ui/array_data_model.js',
          '../../common/js/async_util.js',
          '../../common/js/progress_center_common.js',
          '../../common/js/util.js',
          '../../common/js/volume_manager_common.js',
          'device_handler.js',
          'drive_sync_handler.js',
          'file_operation_handler.js',
          'file_operation_manager.js',
          'progress_center.js',
          'test_util.js',
          'volume_manager.js',
          '../../../../webui/resources/js/cr/ui/dialogs.js',
        ],
        'externs': ['<(CLOSURE_DIR)/externs/chrome_send_externs.js'],
      },
      'includes': [
        '../../../../../third_party/closure_compiler/compile_js.gypi'
      ],
    }
  ],
}
