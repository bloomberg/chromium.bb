# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'include_dirs': [
      '../..',  # add it first, so src/base headers are used instead of the ones
                # brought with the library as cc files would be taken from the
                # main chrome tree as well.
      'src',
      'src/test',
      # The libphonenumber source (and test code) expects the
      # generated protocol headers to be available with "phonenumbers" include
      # path, e.g. #include "phonenumbers/foo.pb.h".
      '<(SHARED_INTERMEDIATE_DIR)/protoc_out/third_party/libphonenumber',
    ],
    'defines': [
      'USE_HASH_MAP=1',
      'USE_GOOGLE_BASE=1',
      'USE_ICU_REGEXP=1',
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
      'src/phonenumbers/asyoutypeformatter.cc',
      'src/phonenumbers/default_logger.cc',
      # Comment next line and uncomment the line after, if complete metadata
      # (with examples) is needed.
      'src/phonenumbers/lite_metadata.cc',
      #'src/phonenumbers/metadata.cc',
      'src/phonenumbers/logger.cc',
      'src/phonenumbers/phonenumber.cc',
      # The following two files should be added on 'as needed' basis.
      #'src/phonenumbers/phonenumbermatch.cc',
      #'src/phonenumbers/phonenumbermatcher.cc',
      'src/phonenumbers/phonenumberutil.cc',
      'src/phonenumbers/regexp_adapter_icu.cc',
      'src/phonenumbers/regexp_cache.cc',
      'src/phonenumbers/stringutil.cc',
      'src/phonenumbers/unicodestring.cc',
      'src/phonenumbers/utf/rune.c',
      'src/phonenumbers/utf/unicodetext.cc',
      'src/phonenumbers/utf/unilib.cc',
      'src/resources/phonemetadata.proto',
      'src/resources/phonenumber.proto',
    ],
    'direct_dependent_settings': {
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)/protoc_out/third_party/libphonenumber',
      ],
    },
    'variables': {
      'proto_in_dir': 'src/resources',
      'proto_out_dir': 'third_party/libphonenumber/phonenumbers',
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
      'src/phonenumbers/test_metadata.cc',
      'src/test/phonenumbers/asyoutypeformatter_test.cc',
      # The following two files should be added on 'as needed' basis.
      #'src/test/phonenumbers/phonenumbermatch_test.cc',
      #'src/test/phonenumbers/phonenumbermatcher_test.cc',
      'src/test/phonenumbers/phonenumberutil_test.cc',
      'src/test/phonenumbers/regexp_adapter_test.cc',
      'src/test/phonenumbers/stringutil_test.cc',
      'src/test/phonenumbers/test_util.cc',
      'src/test/phonenumbers/unicodestring_test.cc',
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
