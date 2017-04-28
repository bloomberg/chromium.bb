# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'actions_controller',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '<(DEPTH)/ui/webui/resources/js/cr/compiled_resources2.gyp:ui',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:context_menu_handler',
        'actions_model',
        'file_selection',
        'ui/compiled_resources2.gyp:file_manager_ui',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'actions_model',
      'dependencies': [
        # TODO(oka): Depend on externs/ instead of background/.
        '../../background/js/compiled_resources2.gyp:drive_sync_handler',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '<(EXTERNS_GYP):file_manager_private',
        'folder_shortcuts_data_model',
        'metadata/compiled_resources2.gyp:metadata_model',
        'ui/compiled_resources2.gyp:error_dialog',
        'ui/compiled_resources2.gyp:files_alert_dialog',
        'ui/compiled_resources2.gyp:list_container',
        'ui/compiled_resources2.gyp:share_dialog',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'app_state_controller',
      'dependencies': [
        'dialog_type',
        'directory_model',
        'ui/compiled_resources2.gyp:file_manager_ui',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'column_visibility_controller',
      'dependencies': [
        '../../common/js/compiled_resources2.gyp:volume_manager_common',
        'directory_model',
        'ui/compiled_resources2.gyp:file_manager_ui',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'constants',
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'dialog_action_controller',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:command',
        'dialog_type',
        'directory_contents',
        'directory_model',
        'file_selection',
        'launch_param',
        'metadata/compiled_resources2.gyp:metadata_model',
        'naming_controller',
        'ui/compiled_resources2.gyp:dialog_footer',
        'volume_manager_wrapper',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'dialog_type',
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'directory_contents',
      'dependencies': [
        '../../common/js/compiled_resources2.gyp:async_util',
        '../../common/js/compiled_resources2.gyp:metrics',
        '../../common/js/compiled_resources2.gyp:util',
        '../../common/js/compiled_resources2.gyp:volume_manager_common',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:array_data_model',
        '<(EXTERNS_GYP):file_manager_private',
        'constants',
        'file_list_model',
        'metadata/compiled_resources2.gyp:metadata_model',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'directory_model',
      'dependencies': [
        # TODO(oka): Depend on externs/ instead of background/.
        '../../background/js/compiled_resources2.gyp:file_operation_manager',
        '../../common/js/compiled_resources2.gyp:importer_common',
        '../../common/js/compiled_resources2.gyp:metrics',
        'directory_contents',
        'file_watcher',
        'ui/compiled_resources2.gyp:file_list_selection_model',
        'volume_manager_wrapper',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'directory_tree_naming_controller',
      'dependencies': [
        '../../common/js/compiled_resources2.gyp:util',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:dialogs',
        'directory_model',
        'ui/compiled_resources2.gyp:directory_tree',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'drop_effect_and_label',
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'elements_importer',
      'dependencies': [
        '<(EXTERNS_GYP):metrics_private',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'empty_folder_controller',
      'dependencies': [
        'directory_model',
        'file_list_model',
        'ui/compiled_resources2.gyp:empty_folder',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'file_list_model',
      'dependencies': [
        '../../common/js/compiled_resources2.gyp:file_type',
        '../../common/js/compiled_resources2.gyp:util',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:array_data_model',
        'metadata/compiled_resources2.gyp:metadata_model',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
#    {
#      'target_name': 'file_manager',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'file_manager_commands',
#      'includes': ['../../../compile_js2.gypi'],
#    },
    {
      'target_name': 'file_selection',
      'dependencies': [
        # TODO(oka): Depend on externs/ instead of background/.
        '../../background/js/compiled_resources2.gyp:file_operation_manager',
        '../../common/js/compiled_resources2.gyp:file_type',
        '../../common/js/compiled_resources2.gyp:util',
        '../../common/js/compiled_resources2.gyp:volume_manager_common',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        'constants',
        'directory_model',
        'metadata/compiled_resources2.gyp:metadata_model',
        'ui/compiled_resources2.gyp:list_container',
        'volume_manager_wrapper',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'file_tasks',
      'dependencies': [
        'directory_model',
        'metadata/compiled_resources2.gyp:metadata_model',
        'ui/compiled_resources2.gyp:file_manager_ui',
        'volume_manager_wrapper',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'file_transfer_controller',
      'dependencies': [
        # TODO(oka): Depend on externs/ instead of background/.
        '../../background/js/compiled_resources2.gyp:file_operation_manager',
        '../../background/js/compiled_resources2.gyp:progress_center',
        'directory_model',
        'drop_effect_and_label',
        'file_selection',
        'metadata/compiled_resources2.gyp:metadata_model',
        'metadata/compiled_resources2.gyp:thumbnail_model',
        'ui/compiled_resources2.gyp:directory_tree',
        'ui/compiled_resources2.gyp:list_container',
        'ui/compiled_resources2.gyp:multi_profile_share_dialog',
        'volume_manager_wrapper',
        'webui_command_extender',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'file_watcher',
      'dependencies': [
        '../../common/js/compiled_resources2.gyp:async_util',
        '../../common/js/compiled_resources2.gyp:util',
        '../../common/js/compiled_resources2.gyp:volume_manager_common',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(EXTERNS_GYP):file_manager_private',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'folder_shortcuts_data_model',
      'dependencies': [
        '../../common/js/compiled_resources2.gyp:async_util',
        '../../common/js/compiled_resources2.gyp:metrics',
        '../../common/js/compiled_resources2.gyp:util',
        '../../common/js/compiled_resources2.gyp:volume_manager_common',
        '<(EXTERNS_GYP):chrome_extensions',
        'volume_manager_wrapper',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
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
    {
      'target_name': 'list_thumbnail_loader',
      'dependencies': [
        '../../common/js/compiled_resources2.gyp:volume_manager_common',
        'directory_model',
        'file_list_model',
        'metadata/compiled_resources2.gyp:thumbnail_model',
        'thumbnail_loader',
        'volume_manager_wrapper',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
#    {
#      'target_name': 'main',
#      'includes': ['../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'main_scripts',
#      'includes': ['../../../compile_js2.gypi'],
#    },
    {
      'target_name': 'main_window_component',
      'dependencies': [
        '../../../externs/compiled_resources2.gyp:directory_change_event',
        'app_state_controller',
        'dialog_type',
        'directory_contents',
        'directory_model',
        'file_selection',
        'naming_controller',
        'task_controller',
        'ui/compiled_resources2.gyp:file_manager_ui',
        'volume_manager_wrapper',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'metadata_box_controller',
      'dependencies': [
        '../../common/js/compiled_resources2.gyp:file_type',
        '../elements/compiled_resources2.gyp:files_metadata_box',
        '../elements/compiled_resources2.gyp:files_quick_view',
        '<(EXTERNS_GYP):file_manager_private',
        'metadata/compiled_resources2.gyp:metadata_model',
        'quick_view_model',
        'ui/compiled_resources2.gyp:file_metadata_formatter',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'metadata_update_controller',
      'dependencies': [
        'directory_model',
        'metadata/compiled_resources2.gyp:metadata_model',
        'ui/compiled_resources2.gyp:file_metadata_formatter',
        'ui/compiled_resources2.gyp:list_container',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'metrics_start',
      'dependencies': [
        '../../common/js/compiled_resources2.gyp:metrics',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'mouse_inactivity_watcher',
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'naming_controller',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:dialogs',
        'directory_contents',
        'directory_model',
        'file_selection',
        'ui/compiled_resources2.gyp:list_container',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'navigation_list_model',
      'dependencies': [
        '../../../externs/compiled_resources2.gyp:volume_info',
        '<(DEPTH)/ui/webui/resources/js/cr/compiled_resources2.gyp:event_target',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:array_data_model',
        'folder_shortcuts_data_model',
        'volume_manager_wrapper',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'progress_center_item_group',
      'dependencies': [
        '../../common/js/compiled_resources2.gyp:progress_center_common',
        '../../common/js/compiled_resources2.gyp:util',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'providers_model',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(EXTERNS_GYP):file_manager_private',
        'volume_manager_wrapper',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'quick_view_controller',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:list_selection_model',
        'dialog_type',
        'file_selection',
        'metadata/compiled_resources2.gyp:metadata_model',
        'metadata_box_controller',
        'quick_view_model',
        'quick_view_uma',
        'task_controller',
        'ui/compiled_resources2.gyp:list_container',
        'volume_manager_wrapper',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'quick_view_model',
      'dependencies': [
        '../../../../../ui/webui/resources/js/compiled_resources2.gyp:cr',
        '../../../../../ui/webui/resources/js/cr/compiled_resources2.gyp:event_target',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'quick_view_uma',
      'dependencies': [
        '../../common/js/compiled_resources2.gyp:file_type',
        'dialog_type',
        'file_tasks',
        'volume_manager_wrapper',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
#    {
#      'target_name': 'scan_controller',
#      'dependencies': [
#        'directory_model',
#        'spinner_controller',
#        'ui/compiled_resources2.gyp:list_container',
#      ],
#      'includes': ['../../../compile_js2.gypi'],
#    },
    {
      'target_name': 'search_controller',
      'dependencies': [
        '../../../externs/compiled_resources2.gyp:volume_manager',
        'directory_model',
        'task_controller',
        'ui/compiled_resources2.gyp:location_line',
        'ui/compiled_resources2.gyp:search_box',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'share_client',
      'dependencies': [
        '../../../externs/compiled_resources2.gyp:entry_location',
        '../../../externs/compiled_resources2.gyp:gallery_foreground',
        '../../../externs/compiled_resources2.gyp:volume_info',
        '../../../externs/compiled_resources2.gyp:volume_info_list',
        '../../../externs/compiled_resources2.gyp:volume_manager',
        '../../../externs/compiled_resources2.gyp:webview_tag',
        '../../common/js/compiled_resources2.gyp:volume_manager_common',
        '<(DEPTH)/ui/webui/resources/js/cr/compiled_resources2.gyp:event_target',
        '<(EXTERNS_GYP):chrome_extensions',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'sort_menu_controller',
      'dependencies': [
        '../elements/compiled_resources2.gyp:files_toggle_ripple',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:context_menu_button',
        'file_list_model',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'spinner_controller',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/cr/compiled_resources2.gyp:event_target',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'task_controller',
      'dependencies': [
        'dialog_type',
        'directory_model',
        'file_selection',
        'file_tasks',
        'metadata/compiled_resources2.gyp:metadata_model',
        'metadata_update_controller',
        'ui/compiled_resources2.gyp:file_manager_ui',
        'volume_manager_wrapper',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
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
    {
      'target_name': 'toolbar_controller',
      'dependencies': [
        '../../common/js/compiled_resources2.gyp:util',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:command',
        'file_selection',
        'ui/compiled_resources2.gyp:list_container',
        'ui/compiled_resources2.gyp:location_line',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'volume_manager_wrapper',
      'dependencies': [
        # TODO(oka): Depend on extern/ instead of background/.
        '../../background/js/compiled_resources2.gyp:volume_manager_factory',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'web_store_utils',
      'dependencies': [
        'constants',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
    {
      'target_name': 'webui_command_extender',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:command',
      ],
      'includes': ['../../../compile_js2.gypi'],
    },
 ],
}
