# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
#    {
#      'target_name': 'actions_submenu',
#      'dependencies': [
#        '../compiled_resources2.gyp:actions_model',
#        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:command',
#      ],
#      'includes': ['../../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'banners',
#      'includes': ['../../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'combobutton',
#      'includes': ['../../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'commandbutton',
#      'includes': ['../../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'default_task_dialog',
#      'includes': ['../../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'dialog_footer',
#      'includes': ['../../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'directory_tree',
#      'includes': ['../../../../compile_js2.gypi'],
#    },
    {
      'target_name': 'drag_selector',
      'dependencies': [
        '../../../../externs/compiled_resources2.gyp:drag_target',
        '<(DEPTH)/ui/webui/resources/js/cr/compiled_resources2.gyp:ui',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:list',
      ],
      'includes': ['../../../../compile_js2.gypi'],
    },
    {
      'target_name': 'empty_folder',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:util',
      ],
      'includes': ['../../../../compile_js2.gypi'],
    },
    {
      'target_name': 'error_dialog',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:dialogs',
      ],
      'includes': ['../../../../compile_js2.gypi'],
    },
#    {
#      'target_name': 'file_grid',
#      'includes': ['../../../../compile_js2.gypi'],
#    },
    {
      'target_name': 'file_list_selection_model',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:list_single_selection_model',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:list_selection_model',
      ],
      'includes': ['../../../../compile_js2.gypi'],
    },
    {
      'target_name': 'file_manager_dialog_base',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '<(DEPTH)/ui/webui/resources/js/cr/compiled_resources2.gyp:ui',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:dialogs',
        '<(EXTERNS_GYP):chrome_extensions',
      ],
      'includes': ['../../../../compile_js2.gypi'],
    },
#    {
#      'target_name': 'file_manager_ui',
#      'includes': ['../../../../compile_js2.gypi'],
#    },
    {
      'target_name': 'file_metadata_formatter',
      'dependencies': [
        '../../../common/js/compiled_resources2.gyp:util',
        '<(DEPTH)/ui/webui/resources/js/cr/compiled_resources2.gyp:event_target',
      ],
      'includes': ['../../../../compile_js2.gypi'],
    },
#    {
#      'target_name': 'file_table',
#      'includes': ['../../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'file_table_list',
#      'includes': ['../../../../compile_js2.gypi'],
#    },
    {
      'target_name': 'files_alert_dialog',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:dialogs',
      ],
      'includes': ['../../../../compile_js2.gypi'],
    },
    {
      'target_name': 'files_confirm_dialog',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:dialogs',
      ],
      'includes': ['../../../../compile_js2.gypi'],
    },
    {
      'target_name': 'files_menu',
      'dependencies': [
        '../../../../externs/compiled_resources2.gyp:paper_elements',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:menu',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:menu_item',
        '<(EXTERNS_GYP):web_animations',
      ],
      'includes': ['../../../../compile_js2.gypi'],
    },
    {
      'target_name': 'gear_menu',
      'dependencies': [
        '../../../common/js/compiled_resources2.gyp:util',
      ],
      'includes': ['../../../../compile_js2.gypi'],
    },
#    {
#      'target_name': 'list_container',
#      'includes': ['../../../../compile_js2.gypi'],
#    },
    {
      'target_name': 'location_line',
      'dependencies': [
        '../../../../externs/compiled_resources2.gyp:platform',
        '../../../../externs/compiled_resources2.gyp:volume_manager',
        '../../../common/js/compiled_resources2.gyp:util',
        '../../../common/js/compiled_resources2.gyp:volume_manager_common',
        '../compiled_resources2.gyp:volume_manager_wrapper',
      ],
      'includes': ['../../../../compile_js2.gypi'],
    },
#    {
#      'target_name': 'multi_profile_share_dialog',
#      'includes': ['../../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'progress_center_panel',
#      'includes': ['../../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'providers_menu',
#      'includes': ['../../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'scrollbar',
#      'includes': ['../../../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'search_box',
#      'includes': ['../../../../compile_js2.gypi'],
#    },
    {
      'target_name': 'share_dialog',
      'dependencies': [
        '../../../common/js/compiled_resources2.gyp:async_util',
        '../../../common/js/compiled_resources2.gyp:util',
        '../compiled_resources2.gyp:share_client',
        '<(EXTERNS_GYP):file_manager_private',
        'file_manager_dialog_base',
      ],
      'includes': ['../../../../compile_js2.gypi'],
    },
#    {
#      'target_name': 'suggest_apps_dialog',
#      'includes': ['../../../../compile_js2.gypi'],
#    },
  ],
}
