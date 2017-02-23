# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
#    {
#      'target_name': 'actions_controller',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'actions_model',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'app_state_controller',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'column_visibility_controller',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'dialog_action_controller',
#      'includes': ['../../../compile_js2.gypi'],
#    },
    {
      'target_name': 'dialog_type',
      'includes': ['../../../compile_js2.gypi'],
    },
#    {
#      'target_name': 'directory_contents',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'directory_model',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'directory_tree_naming_controller',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'elements_importer',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'empty_folder_controller',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'file_list_model',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'file_manager',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'file_manager_commands',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'file_selection',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'file_tasks',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'file_transfer_controller',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'file_watcher',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'folder_shortcuts_data_model',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'gear_menu_controller',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'import_controller',
#      'includes': ['../../../compile_js2.gypi'],
#    },
    {
      'target_name': 'launch_param',
      'dependencies': [
        '../../common/js/compiled_resources2.gyp:volume_manager_common',
        'dialog_type',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
#    {
#      'target_name': 'list_thumbnail_loader',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'main',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'main_scripts',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'main_window_component',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'metadata_box_controller',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'metadata_update_controller',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'metrics_start',
#      'includes': ['../../../compile_js2.gypi'],
#    },
    {
      'target_name': 'mouse_inactivity_watcher',
      'includes': ['../../../compile_js2.gypi'],
    },
#    {
#      'target_name': 'naming_controller',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'navigation_list_model',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'progress_center_item_group',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'providers_model',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'quick_view_controller',
#      'includes': ['../../../compile_js2.gypi'],
#    },
    {
      'target_name': 'quick_view_model',
      'dependencies': [
        '../../../../../ui/webui/resources/js/compiled_resources2.gyp:cr',
        '../../../../../ui/webui/resources/js/cr/compiled_resources2.gyp:event_target',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
#    {
#      'target_name': 'quick_view_uma',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'scan_controller',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'search_controller',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'share_client',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'sort_menu_controller',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'spinner_controller',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'task_controller',
#      'includes': ['../../../compile_js2.gypi'],
#    },
    {
      'target_name': 'thumbnail_loader',
      'dependencies': [
        '../../../image_loader/compiled_resources2.gyp:image_loader_client',
        '../../common/js/compiled_resources2.gyp:file_type',
        '../../common/js/compiled_resources2.gyp:util',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
#    {
#      'target_name': 'toolbar_controller',
#      'includes': ['../../../compile_js2.gypi'],
#    },
    {
      'target_name': 'volume_manager_wrapper',
      'dependencies': [
        '../../background/js/compiled_resources2.gyp:volume_manager_factory',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
 ],
}
