# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'url',
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
        'url_canon.h',
        'url_canon_etc.cc',
        'url_canon_fileurl.cc',
        'url_canon_filesystemurl.cc',
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
        'url_canon_stdstring.cc',
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
      'conditions': [
        ['component=="shared_library"',
          {
            'defines': [
              'GURL_DLL',
              'GURL_IMPLEMENTATION=1',
            ],
            'direct_dependent_settings': {
              'defines': [
                'GURL_DLL',
              ],
            },
          }
        ],
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    {
      'target_name': 'url_unittests',
      'type': 'executable',
      'dependencies': [
        'url',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:run_all_unittests',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icuuc',
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
                'variables': {
                  'url_copy_target': '<(PRODUCT_DIR)/url_unittests<(EXECUTABLE_SUFFIX)',
                  'url_copy_dest': '<(PRODUCT_DIR)/googleurl_unittests<(EXECUTABLE_SUFFIX)',
                },
                'inputs': ['<(url_copy_target)'],
                'outputs': ['<(url_copy_dest)'],
                'action_name': 'TEMP_copy_url_unittests',
                'action': [
                  'python', '-c',
                  'import os, shutil, stat; ' \
                  'shutil.copyfile(\'<(url_copy_target)\', \'<(url_copy_dest)\'); ' \
                  'os.chmod(\'<(url_copy_dest)\', ' \
                  'stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)'
                ],
              },
            ],
          }
        ],
        ['OS == "ios"',
          {
            'actions': [
              {
                'message': 'TEMPORARY: Copying url_unittests to googleurl_unittests',
                'variables': {
                  'url_copy_target': '<(PRODUCT_DIR)/url_unittests.app/url_unittests',
                  'url_copy_dest': '<(PRODUCT_DIR)/googleurl_unittests.app/googleurl_unittests',
                },
                'inputs': ['<(url_copy_target)'],
                'outputs': ['<(url_copy_dest)'],
                'action_name': 'TEMP_copy_url_unittests',
                'action': [
                  'python', '-c',
                  'import os, shutil, stat; ' \
                  'shutil.copyfile(\'<(url_copy_target)\', \'<(url_copy_dest)\'); ' \
                  'os.chmod(\'<(url_copy_dest)\', ' \
                  'stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)'
                ],
              },
            ],
          }
        ]
      ],
    },
  ],
}
