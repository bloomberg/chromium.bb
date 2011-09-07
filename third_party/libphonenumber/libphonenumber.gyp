# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'include_dirs': [
      '../..',  # add it first, so src/base headers are used instead of the ones
                # brought with the library as cc files would be taken from the
                # main chrome tree as well.
      'cpp/src',
      # The libphonenumber source (and test code) expects the
      # generated protocol headers to be available with no include
      # path, e.g. #include "foo.pb.h".
      '<(SHARED_INTERMEDIATE_DIR)/protoc_out/third_party/libphonenumber',
    ],
    'defines': [
      'USE_HASH_MAP=1',
      'USE_GOOGLE_BASE=1',
    ],
  },
  'includes': [
    '../../build/win_precompile.gypi',
  ],
  'targets': [{
    'target_name': 'libphonenumber',
    'type': 'static_library',
    'dependencies': [
      '../icu/icu.gyp:icui18n',
      '../icu/icu.gyp:icuuc',
      '../../base/base.gyp:base',
      '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
    ],
    'sources': [
      # 'chrome/regexp_adapter_icuregexp.cc',
      'cpp/src/default_logger.cc',
      'cpp/src/lite_metadata.cc',
      'cpp/src/logger.cc',
      'cpp/src/phonenumber.cc',
      'cpp/src/phonenumberutil.cc',
      'cpp/src/regexp_adapter_icu.cc',
      'cpp/src/regexp_cache.cc',
      'cpp/src/stringutil.cc',
      'cpp/src/utf/rune.c',
      'cpp/src/utf/unicodetext.cc',
      'cpp/src/utf/unilib.cc',
      'resources/phonemetadata.proto',
      'resources/phonenumber.proto',
    ],
    'direct_dependent_settings': {
      'include_dirs': [
        # The libphonenumber headers expect generated protocol headers
        # to be available with no include path, e.g. #include
        # "foo.pb.h".
        '<(SHARED_INTERMEDIATE_DIR)/protoc_out/third_party/libphonenumber',
      ],
    },
    'variables': {
      'proto_in_dir': 'resources',
      'proto_out_dir': 'third_party/libphonenumber',
    },
    'includes': [ '../../build/protoc.gypi' ],
    'conditions': [
      ['OS=="win"', {
        'action': [
          '/wo4309',
        ],
      }],
    ],
  },
  {
    'target_name': 'libphonenumber_unittests',
    'type': 'executable',
    'sources': [
      '../../base/test/run_all_unittests.cc',
      'cpp/src/phonenumberutil_test.cc',
      'cpp/src/regexp_adapter_test.cc',
      'cpp/src/stringutil_test.cc',
      'cpp/src/test_metadata.cc',
    ],
    'dependencies': [
      '../icu/icu.gyp:icui18n',
      '../icu/icu.gyp:icuuc',
      '../../base/base.gyp:base',
      '../../base/base.gyp:test_support_base',
      '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      '../../testing/gmock.gyp:gmock',
      '../../testing/gtest.gyp:gtest',
      'libphonenumber',
    ],
    'conditions': [
      ['OS=="win"', {
        'action': [
          '/wo4309',
        ],
      }],
    ],
  }]
}
