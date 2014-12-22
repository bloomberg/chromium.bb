# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'background',
      'variables': {
        'depends': [
          '../../../webui/resources/js/cr.js',
          '../../../webui/resources/js/cr/event_target.js',
          '../../../webui/resources/js/cr/ui/array_data_model.js',
          '../../../webui/resources/js/load_time_data.js',
          '../../file_manager/common/js/util.js',
          '../../file_manager/common/js/async_util.js',
          '../../file_manager/common/js/file_type.js',
          '../../file_manager/common/js/volume_manager_common.js',
          '../../file_manager/background/js/volume_manager.js',
          'error_util.js',
          'test_util.js',
        ],
        'externs': [
          '<(CLOSURE_DIR)/externs/chrome_send_externs.js',
          '<(CLOSURE_DIR)/externs/chrome_extensions.js',
          '<(CLOSURE_DIR)/externs/file_manager_private.js',
        ],
      },
      'includes': [
        '../../../../third_party/closure_compiler/compile_js.gypi'
      ],
    },
    {
      'target_name': 'video_player',
      'variables': {
        'depends': [
          'error_util.js',
          '../../file_manager/foreground/js/metrics_base.js',
          'video_player_metrics.js',
          '../../../webui/resources/js/cr.js',
          '../../../webui/resources/js/load_time_data.js',
          '../../../webui/resources/js/event_tracker.js',
          '../../../webui/resources/js/cr/ui.js',
          '../../../webui/resources/js/cr/event_target.js',
          '../../../webui/resources/js/cr/ui/array_data_model.js',
          '../../../webui/resources/js/cr/ui/position_util.js',
          '../../../webui/resources/js/cr/ui/menu_item.js',
          '../../../webui/resources/js/cr/ui/menu.js',
          '../../../webui/resources/js/cr/ui/menu_button.js',
          '../../../webui/resources/js/cr/ui/context_menu_handler.js',
          '../../../webui/resources/js/load_time_data.js',
          '../../../webui/resources/js/i18n_template_no_process.js',
          '../../file_manager/common/js/async_util.js',
          '../../file_manager/common/js/file_type.js',
          '../../file_manager/common/js/util.js',
          '../../file_manager/common/js/volume_manager_common.js',
          '../../file_manager/foreground/js/mouse_inactivity_watcher.js',
          '../../file_manager/foreground/js/volume_manager_wrapper.js',
          'cast/cast_extension_discoverer.js',
          'cast/cast_video_element.js',
          'cast/media_manager.js',
          'cast/caster.js',
          'media_controls.js',
        ],
        'externs': [
          '<(CLOSURE_DIR)/externs/chrome_send_externs.js',
          '<(CLOSURE_DIR)/externs/chrome_extensions.js',
          '<(CLOSURE_DIR)/externs/file_manager_private.js',
          '<(CLOSURE_DIR)/externs/media_player_private.js',
          '../../externs/chrome_cast.js',
        ],
      },
      'includes': [
        '../../../../third_party/closure_compiler/compile_js.gypi'
      ],
    }
  ],
}

