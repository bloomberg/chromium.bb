# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //ui/oobe:resources
      'target_name': 'oobe_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/oobe',
      },
      'actions': [
        {
          'action_name': 'oobe_resources',
          'variables': {
            'grit_grd_file': 'oobe_resources.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            '<(grit_out_dir)/oobe_resources.pak',
          ],
        },
      ],
    },
    {
      'variables': {
        'declaration_file': 'declarations/oobe.json',
      },
      'includes': ['../../components/webui_generator/generator/wug.gypi'],
    },
    {
      # GN version: //ui/oobe
      'target_name': 'oobe',
      'type': '<(component)',
      'defines': [
        'OOBE_IMPLEMENTATION'
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/content/content.gyp:content_browser',
        '<(DEPTH)/components/components.gyp:webui_generator',
        'oobe_resources',
	'oobe_wug_generated',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/ui/oobe/grit/oobe_resources_map.cc',
        'oobe_md_ui.cc',
        'oobe_md_ui.h',
        'oobe_export.h',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/content/content.gyp:content_browser',
      ]
    },
  ]
}
