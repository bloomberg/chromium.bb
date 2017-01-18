# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
#    {
#      'target_name': 'app_window_wrapper',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'background',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'background_base',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'device_handler',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'drive_sync_handler',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'duplicate_finder',
#      'includes': ['../../../compile_js2.gypi'],
#    },
    {
      'target_name': 'entry_location_impl',
      'dependencies': [
        '../../../externs/compiled_resources2.gyp:entry_location',
        '../../common/js/compiled_resources2.gyp:volume_manager_common',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
#    {
#      'target_name': 'file_operation_handler',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'file_operation_manager',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'file_operation_util',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'import_history',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'launcher_search',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'media_import_handler',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'media_scanner',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'mock_background',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'mock_file_operation_manager',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'mock_media_scanner',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'mock_progress_center',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'mock_volume_manager',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'progress_center',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'task_queue',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'test_duplicate_finder',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'test_import_history',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'test_util',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'test_util_base',
#      'includes': ['../../../compile_js2.gypi'],
#    },
    {
      'target_name': 'volume_info_impl',
      'dependencies': [
        '../../../externs/compiled_resources2.gyp:platform',
        '../../../externs/compiled_resources2.gyp:volume_info',
        '../../common/js/compiled_resources2.gyp:volume_manager_common',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'volume_info_list_impl',
      'dependencies': [
        '../../../externs/compiled_resources2.gyp:volume_info',
        '../../../externs/compiled_resources2.gyp:volume_info_list',
        '../../common/js/compiled_resources2.gyp:util',
        './compiled_resources2.gyp:volume_info_impl',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '<(DEPTH)/ui/webui/resources/js/cr/compiled_resources2.gyp:ui',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:array_data_model',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'volume_manager_impl',
      'dependencies': [
        '../../../externs/compiled_resources2.gyp:volume_manager',
        '../../common/js/compiled_resources2.gyp:async_util',
        'volume_info_list_impl',
        'volume_manager_util',
        'entry_location_impl',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'volume_manager_factory',
      'dependencies': [
        '../../../externs/compiled_resources2.gyp:volume_manager',
        'volume_manager_impl',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'volume_manager_util',
      'dependencies': [
        '../../common/js/compiled_resources2.gyp:metrics',
        '../../common/js/compiled_resources2.gyp:metrics_events',
        '../../common/js/compiled_resources2.gyp:util',
        '../../common/js/compiled_resources2.gyp:volume_manager_common',
        'volume_info_impl',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
  ],
}
