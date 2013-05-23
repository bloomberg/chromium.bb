# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'url',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/icu/icu.gyp:icudata',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'sources': [
        'gurl.cc',
        'gurl.h',
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
        'url_parse.cc',
        'url_parse.h',
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
        'FULL_FILESYSTEM_URL_SUPPORT=1',
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
        'url',
      ],
      'sources': [
        'gurl_unittest.cc',
        'url_canon_unittest.cc',
        'url_parse_unittest.cc',
        'url_test_utils.h',
        'url_util_unittest.cc',
      ],
      'defines': [
        'FULL_FILESYSTEM_URL_SUPPORT=1',
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
    {
       # url_unittests was formerly named googleurl_unittests. While the build
       # bots are being switched to use the new name we need to support both
       # executables.
       # TODO(tfarina): Remove this target when build bots are building and
       # running url_unittests. crbug.com/229660
      'target_name': 'googleurl_unittests',
      'type': 'none',
      'dependencies': [
        'url_unittests',
      ],
      'conditions': [
        ['OS != "ios"',
          {
            'actions': [
              {
                'message': 'TEMPORARY: Copying url_unittests to googleurl_unittests',
                'action_name': 'copy_url_unittests',
                'variables': {
                  'source_file': '<(PRODUCT_DIR)/url_unittests<(EXECUTABLE_SUFFIX)',
                  'dest_file': '<(PRODUCT_DIR)/googleurl_unittests<(EXECUTABLE_SUFFIX)',
                },
                'inputs': [
                  '../build/cp.py',
                  '<(source_file)',
                ],
                'outputs': [
                  '<(dest_file)',
                ],
                'action': [
                  'python', '../build/cp.py', '<(source_file)', '<(dest_file)',
                ],
              },
            ],
          }, {  # else OS == "ios"
            'actions': [
              {
                'message': 'TEMPORARY: Copying url_unittests to googleurl_unittests',
                'action_name': 'copy_url_unittests',
                'variables': {
                  'source_file': '<(PRODUCT_DIR)/url_unittests.app/',
                  'dest_file': '<(PRODUCT_DIR)/googleurl_unittests.app',
                },
                'inputs': [
                  '../build/cp.py',
                  '<(source_file)',
                ],
                'outputs': [
                  '<(dest_file)',
                ],
                'action': [
                  'cp', '-R', '<(source_file)', '<(dest_file)',
                ],
              },
            ],
          }
        ]
      ],
    },
  ],
}
