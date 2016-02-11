# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'type': '<(gtest_target_type)',
  'sources': [
    '<@(libaddressinput_test_files)',
    'chromium/addressinput_util_unittest.cc',
    'chromium/chrome_address_validator_unittest.cc',
    'chromium/chrome_metadata_source_unittest.cc',
    'chromium/chrome_storage_impl_unittest.cc',
    'chromium/fallback_data_store_unittest.cc',
    'chromium/storage_test_runner.cc',
    'chromium/string_compare_unittest.cc',
    'chromium/trie_unittest.cc',
  ],
  'include_dirs': [
    '../../',
    'src/cpp/src/',
  ],
  'dependencies': [
    '../../base/base.gyp:run_all_unittests',
    '../../components/prefs/prefs.gyp:prefs',
    '../../net/net.gyp:net_test_support',
    '../../testing/gtest.gyp:gtest',
    'libaddressinput',
    'libaddressinput_util',
  ],
}
