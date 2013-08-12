# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # Note, this target_name cannot be 'url', because that will generate
      # 'url.dll' for a Windows component build, and that will confuse Windows,
      # which has a system DLL with the same name.
      'target_name': 'url_lib',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/icu/icu.gyp:icudata',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'sources': [
        'gurl.cc',
        'gurl.h',
        'third_party/mozilla/url_parse.cc',
        'third_party/mozilla/url_parse.h',
        'url_canon.h',
        'url_canon_etc.cc',
        'url_canon_filesystemurl.cc',
        'url_canon_fileurl.cc',
        'url_canon_host.cc',
        'url_canon_icu.cc',
        'url_canon_icu.h',
        'url_canon_internal.cc',
        'url_canon_internal.h',
        'url_canon_internal_file.h',
        'url_canon_ip.cc',
        'url_canon_ip.h',
        'url_canon_mailtourl.cc',
        'url_canon_path.cc',
        'url_canon_pathurl.cc',
        'url_canon_query.cc',
        'url_canon_relative.cc',
        'url_canon_stdstring.h',
        'url_canon_stdurl.cc',
        'url_file.h',
        'url_parse_file.cc',
        'url_parse_internal.h',
        'url_util.cc',
        'url_util.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'defines': [
        'URL_IMPLEMENTATION',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    {
      'target_name': 'url_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:run_all_unittests',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icuuc',
        'url_lib',
      ],
      'sources': [
        'gurl_unittest.cc',
        'url_canon_unittest.cc',
        'url_parse_unittest.cc',
        'url_test_utils.h',
        'url_util_unittest.cc',
      ],
      'conditions': [
        ['os_posix==1 and OS!="mac" and OS!="ios"',
          {
            'conditions': [
              ['linux_use_tcmalloc==1',
                {
                  'dependencies': [
                    '../base/allocator/allocator.gyp:allocator',
                  ],
                }
              ],
            ],
          }
        ],
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
  ],
}
