# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'background',
      'variables': {
        'depends': [
          '../../../../../third_party/jstemplate/compiled_resources.gyp:jstemplate',
          '../../../../webui/resources/js/load_time_data.js',
          '../../../../webui/resources/js/cr.js',
          '../../../../webui/resources/js/util.js',
          '../../../../webui/resources/js/cr/event_target.js',
          '../../../../webui/resources/js/cr/ui/array_data_model.js',
          '../../../../webui/resources/js/cr/ui/dialogs.js',
          '../../../image_loader/image_loader_client.js',
          '../../common/js/error_util.js',
          '../../common/js/async_util.js',
          '../../common/js/progress_center_common.js',
          '../../common/js/util.js',
          '../../common/js/volume_manager_common.js',
          '../../foreground/js/ui/progress_center_panel.js',
          '../../foreground/js/progress_center_item_group.js',
          '../../foreground/js/metadata/byte_reader.js',
          'app_window_wrapper.js',
          'device_handler.js',
          'drive_sync_handler.js',
          'file_operation_handler.js',
          'file_operation_manager.js',
          'import_history.js',
          'progress_center.js',
          'volume_manager.js',
          'background_base.js',
        ],
        'externs': [
          '<(CLOSURE_DIR)/externs/chrome_send_externs.js',
          '<(CLOSURE_DIR)/externs/chrome_extensions.js',
          '<(CLOSURE_DIR)/externs/command_line_private.js',
          '<(CLOSURE_DIR)/externs/file_manager_private.js',
          '<(CLOSURE_DIR)/externs/metrics_private.js',
          '../../../externs/chrome_test.js',
          '../../../externs/connection.js',
          '../../../externs/css_rule.js',
          '../../../externs/webview_tag.js',
          '../../common/js/externs.js',
        ],
      },
      'includes': [
        '../../../../../third_party/closure_compiler/compile_js.gypi'
      ],
    }
  ],
}
