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
    'defines': [
      'CUSTOM_BASICTYPES="base/basictypes.h"',
      'CUSTOM_SCOPED_PTR="base/memory/scoped_ptr.h"',
    ],
  },
  'targets': [
    {
      'target_name': 'libaddressinput_strings',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/third_party/libaddressinput/',
      },
      'actions': [
        {
          'action_name': 'libaddressinput_strings',
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
        'chromium/chrome_downloader_impl.cc',
        'chromium/chrome_downloader_impl.h',
        'chromium/chrome_storage_impl.cc',
        'chromium/chrome_storage_impl.h',
        'chromium/json.cc',
        'chromium/string_compare.cc',
        '<(libaddressinput_dir)/cpp/include/libaddressinput/address_data.h',
        '<(libaddressinput_dir)/cpp/include/libaddressinput/address_field.h',
        '<(libaddressinput_dir)/cpp/include/libaddressinput/address_problem.h',
        '<(libaddressinput_dir)/cpp/include/libaddressinput/address_ui_component.h',
        '<(libaddressinput_dir)/cpp/include/libaddressinput/address_ui.h',
        '<(libaddressinput_dir)/cpp/include/libaddressinput/address_validator.h',
        '<(libaddressinput_dir)/cpp/include/libaddressinput/load_rules_delegate.h',
        '<(libaddressinput_dir)/cpp/include/libaddressinput/util/basictypes.h',
        '<(libaddressinput_dir)/cpp/include/libaddressinput/util/internal/basictypes.h',
        '<(libaddressinput_dir)/cpp/include/libaddressinput/util/internal/move.h',
        '<(libaddressinput_dir)/cpp/include/libaddressinput/util/internal/scoped_ptr.h',
        '<(libaddressinput_dir)/cpp/include/libaddressinput/util/internal/template_util.h',
        '<(libaddressinput_dir)/cpp/include/libaddressinput/util/scoped_ptr.h',
        '<(libaddressinput_dir)/cpp/src/address_data.cc',
        '<(libaddressinput_dir)/cpp/src/address_field.cc',
        '<(libaddressinput_dir)/cpp/src/address_problem.cc',
        '<(libaddressinput_dir)/cpp/src/address_ui.cc',
        '<(libaddressinput_dir)/cpp/src/address_validator.cc',
        '<(libaddressinput_dir)/cpp/src/country_rules_aggregator.cc',
        '<(libaddressinput_dir)/cpp/src/country_rules_aggregator.h',
        '<(libaddressinput_dir)/cpp/src/fallback_data_store.cc',
        '<(libaddressinput_dir)/cpp/src/fallback_data_store.h',
        '<(libaddressinput_dir)/cpp/src/grit.h',
        '<(libaddressinput_dir)/cpp/src/region_data_constants.cc',
        '<(libaddressinput_dir)/cpp/src/region_data_constants.h',
        '<(libaddressinput_dir)/cpp/src/retriever.cc',
        '<(libaddressinput_dir)/cpp/src/retriever.h',
        '<(libaddressinput_dir)/cpp/src/rule.cc',
        '<(libaddressinput_dir)/cpp/src/rule.h',
        '<(libaddressinput_dir)/cpp/src/ruleset.cc',
        '<(libaddressinput_dir)/cpp/src/ruleset.h',
        '<(libaddressinput_dir)/cpp/src/time_to_string.cc',
        '<(libaddressinput_dir)/cpp/src/time_to_string.h',
        '<(libaddressinput_dir)/cpp/src/util/json.h',
        '<(libaddressinput_dir)/cpp/src/util/md5.cc',
        '<(libaddressinput_dir)/cpp/src/util/md5.h',
        '<(libaddressinput_dir)/cpp/src/util/stl_util.h',
        '<(libaddressinput_dir)/cpp/src/util/string_compare.h',
        '<(libaddressinput_dir)/cpp/src/util/string_split.cc',
        '<(libaddressinput_dir)/cpp/src/util/string_split.h',
      ],
      'defines': [
        'VALIDATION_DATA_URL="https://i18napis.appspot.com/ssl-aggregate-address/"',
      ],
      'dependencies': [
        'libaddressinput_strings',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        '<(DEPTH)/third_party/re2/re2.gyp:re2',
      ],
      'direct_dependent_settings': {
        'defines': [
          'CUSTOM_BASICTYPES="base/basictypes.h"',
          'CUSTOM_SCOPED_PTR="base/memory/scoped_ptr.h"',
        ],
        'include_dirs': [
          '<(libaddressinput_dir)/cpp/include/',
        ],
      },
    },
    {
      'target_name': 'libaddressinput_unittests',
      'type': '<(gtest_target_type)',
      'include_dirs': [
        '<(DEPTH)',
        '<(libaddressinput_dir)/cpp/src/',
        '<(DEPTH)/testing/gtest/include/',
        '<(SHARED_INTERMEDIATE_DIR)/libaddressinput/',
      ],
      'sources': [
        'chromium/chrome_downloader_impl_unittest.cc',
        'chromium/chrome_rule_test.cc',
        'chromium/chrome_storage_impl_unittest.cc',
        '<(libaddressinput_dir)/cpp/test/address_data_test.cc',
        '<(libaddressinput_dir)/cpp/test/address_ui_test.cc',
        '<(libaddressinput_dir)/cpp/test/address_validator_test.cc',
        '<(libaddressinput_dir)/cpp/test/country_rules_aggregator_test.cc',
        '<(libaddressinput_dir)/cpp/test/fake_downloader.cc',
        '<(libaddressinput_dir)/cpp/test/fake_downloader.h',
        '<(libaddressinput_dir)/cpp/test/fake_downloader_test.cc',
        '<(libaddressinput_dir)/cpp/test/fake_storage.cc',
        '<(libaddressinput_dir)/cpp/test/fake_storage.h',
        '<(libaddressinput_dir)/cpp/test/fake_storage_test.cc',
        '<(libaddressinput_dir)/cpp/test/fallback_data_store_test.cc',
        '<(libaddressinput_dir)/cpp/test/region_data_constants_test.cc',
        '<(libaddressinput_dir)/cpp/test/retriever_test.cc',
        '<(libaddressinput_dir)/cpp/test/rule_test.cc',
        '<(libaddressinput_dir)/cpp/test/storage_test_runner.cc',
        '<(libaddressinput_dir)/cpp/test/storage_test_runner.h',
        '<(libaddressinput_dir)/cpp/test/util/json_test.cc',
        '<(libaddressinput_dir)/cpp/test/util/md5_unittest.cc',
        '<(libaddressinput_dir)/cpp/test/util/scoped_ptr_unittest.cc',
        '<(libaddressinput_dir)/cpp/test/util/stl_util_unittest.cc',
        '<(libaddressinput_dir)/cpp/test/util/string_split_unittest.cc',
      ],
      'defines': [
        'TEST_DATA_DIR="third_party/libaddressinput/src/testdata"',
      ],
      'dependencies': [
        'libaddressinput',
        'libaddressinput_strings',
        '<(DEPTH)/base/base.gyp:base_prefs',
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/net/net.gyp:net_test_support',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },
  ],
}
