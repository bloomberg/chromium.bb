# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    # In component mode (shared_lib), we build all of skia as a single DLL.
    # However, in the static mode, we need to build skia as multiple targets
    # in order to support the use case where a platform (e.g. Android) may
    # already have a copy of skia as a system library.
    ['component=="static_library" and use_system_skia==0', {
      'targets': [
        {
          'target_name': 'skia_library',
          'type': 'static_library',
          'includes': [
            'skia_library.gypi',
            'skia_common.gypi',
          ],
        },
      ],
    }],
    ['component=="static_library" and use_system_skia==1', {
      'targets': [
        {
          'target_name': 'skia_library',
          'type': 'none',
          'includes': ['skia_system.gypi'],
        },
      ],
    }],
    ['component=="static_library"', {
      'targets': [
        {
          'target_name': 'skia',
          'type': 'none',
          'dependencies': [
            'skia_library',
            'skia_chrome',
          ],
          'export_dependent_settings': [
            'skia_library',
            'skia_chrome',
          ],
        },
        {
          'target_name': 'skia_chrome',
          'type': 'static_library',
          'includes': [
            'skia_chrome.gypi',
            'skia_common.gypi',
          ],
        },
      ],
    },
    {  # component != static_library
      'targets': [
        {
          'target_name': 'skia',
          'type': 'shared_library',
          'includes': [
            'skia_library.gypi',
            'skia_chrome.gypi',
            'skia_common.gypi',
          ],
          'defines': [
            'SKIA_DLL',
            'SKIA_IMPLEMENTATION=1',
          ],
          'direct_dependent_settings': {
            'defines': [
              'SKIA_DLL',
            ],
          },
        },
        {
          'target_name': 'skia_library',
          'type': 'none',
        },
        {
          'target_name': 'skia_chrome',
          'type': 'none',
        },
      ],
    }],
  ],

  # targets that are not dependent upon the component type
  'targets': [
    {
      'target_name': 'skia_chrome_opts',
      'type': 'static_library',
      'include_dirs': [
        '..',
        'config',
        '../third_party/skia/include/config',
        '../third_party/skia/include/core',
      ],
      'conditions': [
        [ 'os_posix == 1 and OS != "mac" and OS != "android" and \
            target_arch != "arm" and target_arch != "mipsel"', {
          'cflags': [
            '-msse2',
          ],
        }],
        [ 'target_arch != "arm" and target_arch != "mipsel"', {
          'sources': [
            'ext/convolver_SSE2.cc',
          ],
        }],
        [ 'target_arch == "mipsel"',{
          'cflags': [
            '-fomit-frame-pointer',
          ],
          'sources': [
            'ext/convolver_mips_dspr2.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'image_operations_bench',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        'skia',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'ext/image_operations_bench.cc',
      ],
    },
  ],
}
