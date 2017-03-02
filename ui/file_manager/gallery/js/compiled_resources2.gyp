# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# TODO(oka): Compile all the targets.
{
  'targets': [
#    {
#      'target_name': 'background',
#      'includes': ['../../compile_js2.gypi'],
#    },
    {
      'target_name': 'dimmable_ui_controller',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(EXTERNS_GYP):chrome_extensions',
        'gallery_constants',
        'image_editor/compiled_resources2.gyp:image_editor_prompt',
      ],
      'includes': ['../../compile_js2.gypi'],
    },
    {
      'target_name': 'entry_list_watcher',
      'dependencies': [
        '../../externs/compiled_resources2.gyp:platform',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:array_data_model',
        '<(EXTERNS_GYP):file_manager_private',
      ],
      'includes': ['../../compile_js2.gypi'],
    },
    {
      'target_name': 'error_banner',
      'dependencies': [
        '../../file_manager/common/js/compiled_resources2.gyp:util',
      ],
      'includes': ['../../compile_js2.gypi'],
    },
#    {
#      'target_name': 'gallery',
#      'includes': ['../../compile_js2.gypi'],
#    },
    {
      'target_name': 'gallery_constants',
      'includes': ['../../compile_js2.gypi'],
    },
#    {
#      'target_name': 'gallery_data_model',
#      'dependencies': [
#        'thumbnail_mode',
#        '../../file_manager/common/js/compiled_resources2.gyp:util',
#        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
#        'gallery_item',
#        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:array_data_model',
#      ],
#      'includes': ['../../compile_js2.gypi'],
#    },
    {
      'target_name': 'gallery_item',
      'dependencies': [
        '../../file_manager/common/js/compiled_resources2.gyp:util',
        '../../file_manager/foreground/js/compiled_resources2.gyp:volume_manager_wrapper',
        '../../file_manager/foreground/js/metadata/compiled_resources2.gyp:metadata_model',
        '../../file_manager/foreground/js/metadata/compiled_resources2.gyp:thumbnail_model',
        'gallery_util',
        'image_editor/compiled_resources2.gyp:image_encoder',
        'image_editor/compiled_resources2.gyp:image_util',
      ],
      'includes': ['../../compile_js2.gypi'],
    },
#    {
#      'target_name': 'gallery_scripts',
#      'includes': ['../../compile_js2.gypi'],
#    },
    {
      'target_name': 'gallery_util',
      'dependencies': [
        '../../file_manager/common/js/compiled_resources2.gyp:file_type',
        '../../file_manager/common/js/compiled_resources2.gyp:util',
        '../../file_manager/common/js/compiled_resources2.gyp:volume_manager_common',
        '../../file_manager/foreground/js/compiled_resources2.gyp:volume_manager_wrapper',
      ],
      'includes': ['../../compile_js2.gypi'],
    },
    {
      'target_name': 'metadata_worker',
      'includes': ['../../compile_js2.gypi'],
    },
    {
      'target_name': 'ribbon',
      'dependencies': [
        '../../externs/compiled_resources2.gyp:gallery_event',
        '../../file_manager/foreground/js/compiled_resources2.gyp:thumbnail_loader',
        '../../file_manager/foreground/js/metadata/compiled_resources2.gyp:thumbnail_model',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:array_data_model',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:list_selection_model',
        'gallery_item',
        'image_editor/compiled_resources2.gyp:image_util',
      ],
      'includes': ['../../compile_js2.gypi'],
    },
#    {
#      'target_name': 'slide_mode',
#      'dependencies': [
#        '../../file_manager/common/js/compiled_resources2.gyp:metrics',
#        '../../file_manager/common/js/compiled_resources2.gyp:util',
#        '<(EXTERNS_GYP):chrome_extensions',
#        'gallery_item',
#        'image_editor/compiled_resources2.gyp:image_editor',
#        'image_editor/compiled_resources2.gyp:image_util',
#        'image_editor/compiled_resources2.gyp:image_view',
#        'image_editor/compiled_resources2.gyp:viewport',
#      ],
#      'includes': ['../../compile_js2.gypi'],
#    },
#    {
#      'target_name': 'thumbnail_mode',
#      'dependencies': [
#        '../../file_manager/foreground/js/compiled_resources2.gyp:thumbnail_loader',
#        '../../file_manager/foreground/js/metadata/compiled_resources2.gyp:thumbnail_model',
#        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:list_selection_model',
#        'error_banner',
#        'gallery_data_model',
#        'gallery_item',
#        'image_editor/compiled_resources2.gyp:image_editor',
#      ],
#      'includes': ['../../compile_js2.gypi'],
#    },
  ],
}
