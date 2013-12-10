# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    # TODO(rouslan): Use the src/ directory. http://crbug.com/327046
    'libaddressinput_dir': 'chromium',
  },
  'target_defaults': {
    'conditions': [
      ['OS=="mac" or OS=="ios"', {
        'xcode_settings': {
          'GCC_WARN_ABOUT_MISSING_NEWLINE': 'NO',
        },
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'generated_messages',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/libaddressinput/',
      },
      'actions': [
        {
          'action_name': 'generate_messages',
          'variables': {
            'grit_grd_file': '<(libaddressinput_dir)/cpp/res/messages.grd',
          },
          'includes': [
            '../../build/grit_action.gypi',
          ],
        },
      ],
      'includes': [
        '../../build/grit_target.gypi',
      ],
    },
    {
      'target_name': 'libaddressinput',
      'type': 'static_library',
      'include_dirs': [
        '<(libaddressinput_dir)/cpp/include/',
        '<(SHARED_INTERMEDIATE_DIR)/libaddressinput/',
      ],
      'sources': [
        'chromium/json.cc',
        '<(libaddressinput_dir)/cpp/include/libaddressinput/address_field.h',
        '<(libaddressinput_dir)/cpp/include/libaddressinput/address_ui_component.h',
        '<(libaddressinput_dir)/cpp/include/libaddressinput/address_ui.h',
        '<(libaddressinput_dir)/cpp/include/libaddressinput/localization.h',
        '<(libaddressinput_dir)/cpp/src/address_field.cc',
        '<(libaddressinput_dir)/cpp/src/address_field_util.cc',
        '<(libaddressinput_dir)/cpp/src/address_field_util.h',
        '<(libaddressinput_dir)/cpp/src/address_ui.cc',
        '<(libaddressinput_dir)/cpp/src/localization.cc',
        '<(libaddressinput_dir)/cpp/src/region_data_constants.cc',
        '<(libaddressinput_dir)/cpp/src/region_data_constants.h',
        '<(libaddressinput_dir)/cpp/src/rule.cc',
        '<(libaddressinput_dir)/cpp/src/rule.h',
        '<(libaddressinput_dir)/cpp/src/util/json.h',
      ],
      'dependencies': [
        'generated_messages',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(libaddressinput_dir)/cpp/include/',
        ],
      },
    },
    {
      'target_name': 'libaddressinput_unittests',
      'type': '<(gtest_target_type)',
      'include_dirs': [
        '<(libaddressinput_dir)/cpp/src/',
        '<(DEPTH)/testing/gtest/include/',
        '<(SHARED_INTERMEDIATE_DIR)/libaddressinput/',
      ],
      'sources': [
        '<(libaddressinput_dir)/cpp/test/address_field_util_test.cc',
        '<(libaddressinput_dir)/cpp/test/address_ui_test.cc',
        '<(libaddressinput_dir)/cpp/test/localization_test.cc',
        '<(libaddressinput_dir)/cpp/test/region_data_constants_test.cc',
        '<(libaddressinput_dir)/cpp/test/rule_test.cc',
        '<(libaddressinput_dir)/cpp/test/util/json_test.cc',
      ],
      'dependencies': [
        'libaddressinput',
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },
  ],
}
