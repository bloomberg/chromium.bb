# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,  # Use higher warning level.
  },
  'target_defaults': {
    'conditions': [
      # Linux shared libraries should always be built -fPIC.
      #
      # TODO(ajwong): For internal pepper plugins, which are statically linked
      # into chrome, do we want to build w/o -fPIC?  If so, how can we express
      # that in the build system?
      ['os_posix==1 and OS!="android" and OS!="mac"', {
        'cflags': ['-fPIC', '-fvisibility=hidden'],

        # This is needed to make the Linux shlib build happy. Without this,
        # -fvisibility=hidden gets stripped by the exclusion in common.gypi
        # that is triggered when a shared library build is specified.
        'cflags/': [['include', '^-fvisibility=hidden$']],
      }],
    ],
  },
  'includes': [
    'ppapi_sources.gypi',
    'ppapi_proxy.gypi',
    'ppapi_shared.gypi',
    'ppapi_tests.gypi',
  ],
  'targets': [
    {
      'target_name': 'ppapi_shared',
      'type': '<(component)',
      'variables': {
        'ppapi_shared_target': 1,
      },
      'dependencies': [
        'ppapi.gyp:ppapi_c',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../gpu/command_buffer/command_buffer.gyp:gles2_utils',
        '../gpu/gpu.gyp:command_buffer_client',
        '../gpu/gpu.gyp:gles2_implementation',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icuuc',
        # TODO(ananta) : The WebKit dependency needs to move to a new target for NACL.
        '<(webkit_src_dir)/Source/WebKit/chromium/WebKit.gyp:webkit',
        '../ui/surface/surface.gyp:surface',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '<(webkit_src_dir)/Source/WebKit/chromium/WebKit.gyp:webkit',
      ],
      'conditions': [
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'ppapi_proxy',
      'type': '<(component)',
      'variables': {
        'ppapi_proxy_target': 1,
      },
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../gpu/gpu.gyp:gles2_implementation',
        '../gpu/gpu.gyp:gpu_ipc',
        '../ipc/ipc.gyp:ipc',
        '../skia/skia.gyp:skia',
        '../ui/surface/surface.gyp:surface',
        'ppapi.gyp:ppapi_c',
        'ppapi_shared',
      ],
      'all_dependent_settings': {
        'include_dirs': [
           '..',
        ],
      },
    },
  ]
}
