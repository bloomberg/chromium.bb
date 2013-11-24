# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'target_defaults': {
    'cflags!': [
      '-fvisibility=hidden',
    ],
    'conditions': [
      ['OS=="mac" or OS=="ios"', {
        'xcode_settings': {
          'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',
          'GCC_WARN_ABOUT_MISSING_NEWLINE': 'NO',
        }
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
            'grit_grd_file': 'src/cpp/res/messages.grd',
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
        'chromium/',
        'src/cpp/include/',
        '<(SHARED_INTERMEDIATE_DIR)/libaddressinput/',
      ],
      'sources': [
        'chromium/util/json.cc',
        'chromium/util/json.h',
        'src/cpp/include/libaddressinput/address_field.h',
        'src/cpp/include/libaddressinput/address_ui_component.h',
        'src/cpp/include/libaddressinput/address_ui.h',
        'src/cpp/include/libaddressinput/localization.h',
        'src/cpp/src/address_field.cc',
        'src/cpp/src/address_field_util.cc',
        'src/cpp/src/address_field_util.h',
        'src/cpp/src/address_ui.cc',
        'src/cpp/src/localization.cc',
        'src/cpp/src/region_data_constants.cc',
        'src/cpp/src/region_data_constants.h',
        'src/cpp/src/rule.cc',
        'src/cpp/src/rule.h',
      ],
      'dependencies': [
        'generated_messages',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'src/cpp/include/',
        ],
      },
    },
    {
      'target_name': 'libaddressinput_unittests',
      'type': '<(gtest_target_type)',
      'include_dirs': [
        'src/cpp/src',
        '<(DEPTH)/testing/gtest/include/',
        '<(SHARED_INTERMEDIATE_DIR)/libaddressinput/',
      ],
      'sources': [
        'src/cpp/test/address_field_util_test.cc',
        'src/cpp/test/address_ui_test.cc',
        'src/cpp/test/localization_test.cc',
        'src/cpp/test/region_data_constants_test.cc',
        'src/cpp/test/rule_test.cc',
        'src/cpp/test/util/json_test.cc',
      ],
      'dependencies': [
        'libaddressinput',
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },
  ],
}
