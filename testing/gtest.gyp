# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'gtest.gypi',
  ],
  'targets': [
    {
      'target_name': 'gtest',
      'toolsets': ['host', 'target'],
      'type': 'static_library',
      'sources': [
        '<@(gtest_sources)',
      ],
      'include_dirs': [
        'gtest',
        'gtest/include',
      ],
      'dependencies': [
        'gtest_prod',
      ],
      'defines': [
        # In order to allow regex matches in gtest to be shared between Windows
        # and other systems, we tell gtest to always use it's internal engine.
        'GTEST_HAS_POSIX_RE=0',
        # Chrome doesn't support / require C++11, yet.
        'GTEST_LANG_CXX11=0',
      ],
      'all_dependent_settings': {
        'defines': [
          'GTEST_HAS_POSIX_RE=0',
          'GTEST_LANG_CXX11=0',
        ],
      },
      'conditions': [
        ['OS == "mac" or OS == "ios"', {
          'sources': [
            'gtest_mac.h',
            'gtest_mac.mm',
            'platform_test_mac.mm',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
          },
        }],
        ['OS == "ios"', {
          'dependencies' : [
            '<(DEPTH)/testing/iossim/iossim.gyp:iossim#host',
          ],
          'direct_dependent_settings': {
            'target_conditions': [
              # Turn all tests into bundles on iOS because that's the only
              # type of executable supported for iOS.
              ['_type=="executable"', {
                'variables': {
                  # Use a variable so the path gets fixed up so it is always
                  # correct when INFOPLIST_FILE finally gets set.
                  'ios_unittest_info_plist_path':
                    '<(DEPTH)/testing/gtest_ios/unittest-Info.plist',
                },
                'mac_bundle': 1,
                'xcode_settings': {
                  'BUNDLE_ID_TEST_NAME': '>(_target_name)',
                  'INFOPLIST_FILE': '>(ios_unittest_info_plist_path)',
                },
                'mac_bundle_resources': [
                  '<(ios_unittest_info_plist_path)',
                  '<(DEPTH)/testing/gtest_ios/Default.png',
                ],
                'mac_bundle_resources!': [
                  '<(ios_unittest_info_plist_path)',
                ],
              }],
            ],
          },
          'sources': [
            'coverage_util_ios.cc',
            'coverage_util_ios.h',
          ],
        }],
        ['OS=="ios" and asan==1', {
          'direct_dependent_settings': {
            'target_conditions': [
              # Package the ASan runtime dylib into the test app bundles.
              ['_type=="executable"', {
                'postbuilds': [
                  {
                    'variables': {
                      # Define copy_asan_dylib_path in a variable ending in
                      # _path so that gyp understands it's a path and
                      # performs proper relativization during dict merging.
                      'copy_asan_dylib_path':
                        '<(DEPTH)/build/mac/copy_asan_runtime_dylib.sh',
                    },
                    'postbuild_name': 'Copy ASan runtime dylib',
                    'action': [
                      '>(copy_asan_dylib_path)',
                    ],
                  },
                ],
              }],
            ],
          },
        }],
        ['OS=="android" and android_app_abi=="x86"', {
          'defines': [
            'GTEST_HAS_CLONE=0',
          ],
          'direct_dependent_settings': {
            'defines': [
              'GTEST_HAS_CLONE=0',
            ],
          },
        }],
      ],
      'direct_dependent_settings': {
        'defines': [
          'UNIT_TEST',
        ],
        'include_dirs': [
          'gtest/include',  # So that gtest headers can find themselves.
        ],
        'target_conditions': [
          ['_type=="executable"', {
            'test': 1,
            'conditions': [
              ['OS=="mac"', {
                'run_as': {
                  'action????': ['${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}'],
                },
              }],
              ['OS=="ios"', {
                'variables': {
                  # Use a variable so the path gets fixed up so it is always
                  # correct when the action finally gets used.
                  'ios_run_unittest_script_path':
                    '<(DEPTH)/testing/gtest_ios/run-unittest.sh',
                },
                'run_as': {
                  'action????': ['>(ios_run_unittest_script_path)'],
                },
              }],
              ['OS=="win"', {
                'run_as': {
                  'action????': ['$(TargetPath)', '--gtest_print_time'],
                },
              }],
            ],
          }],
        ],
        'msvs_disabled_warnings': [4800],
      },
    },
    {
      'target_name': 'gtest_main',
      'type': 'static_library',
      'dependencies': [
        'gtest',
      ],
      'sources': [
        'gtest/src/gtest_main.cc',
      ],
    },
    {
      'target_name': 'gtest_prod',
      'toolsets': ['host', 'target'],
      'type': 'none',
      'sources': [
        'gtest/include/gtest/gtest_prod.h',
      ],
    },
  ],
}
