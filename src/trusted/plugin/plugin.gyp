# Copyright (c) 2009 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'plugin.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="npNaClPlugin"', {
        'sources': [
          '<@(common_sources)',
          '<@(npapi_sources)',
          'npapi/video.cc',
        ],
        'xcode_settings': {
          'WARNING_CFLAGS!': [
            # TODO(bradnelson): remove -pedantic when --std=c++98 in common.gypi
            '-pedantic',
          ],
          'WARNING_CFLAGS': [
            '-Wno-deprecated',
            '-Wno-deprecated-declarations',
          ],
        },
        'conditions': [
          ['OS=="win"', {
            'sources': [
              'win/nacl_plugin.rc',
            ],
            'msvs_settings': {
              'VCCLCompilerTool': {
                'ExceptionHandling': '2',  # /EHsc
              },
              'VCLinkerTool': {
                'AdditionalLibraryDirectories': [
                   '$(OutDir)/lib',
                ],
              },
            },
          }],
        ],
      }],
      ['target_base=="ppNaClPlugin"', {
        'sources': [
          '<@(common_sources)',
          '<@(ppapi_sources)',
        ],
        'defines': [
          'NACL_PPAPI',
        ],
        'xcode_settings': {
          'WARNING_CFLAGS!': [
            # TODO(bradnelson): remove -pedantic when --std=c++98 in common.gypi
            '-pedantic',
          ],
          'WARNING_CFLAGS': [
            '-Wno-deprecated',
            '-Wno-deprecated-declarations',
          ],
        },
        'conditions': [
          ['OS=="win"', {
            'sources': [
              'win/nacl_plugin.rc',
            ],
            'msvs_settings': {
              'VCCLCompilerTool': {
                'ExceptionHandling': '2',  # /EHsc
              },
              'VCLinkerTool': {
                'AdditionalLibraryDirectories': [
                   '$(OutDir)/lib',
                ],
              },
            },
          }],
        ],
      }],
    ],
  },
  'targets': [],
  'conditions': [
    ['target_arch!="x64"', {
      'targets': [
      # ----------------------------------------------------------------------
        {
          'target_name': 'npGoogleNaClPlugin',
          'type': 'shared_library',
          'variables': {
            'target_base': 'npNaClPlugin',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio',
            '<(DEPTH)/native_client/src/shared/imc/imc.gyp:google_nacl_imc_c',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
            '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc',
            '<(DEPTH)/native_client/src/shared/npruntime/npruntime.gyp:google_nacl_npruntime',
            '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
            '<(DEPTH)/native_client/src/trusted/nonnacl_util/nonnacl_util.gyp:nonnacl_util',
            '<(DEPTH)/native_client/src/trusted/platform_qualify/platform_qualify.gyp:platform_qual_lib',
            '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:expiration',
            '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:gio_wrapped_desc',
          ],
          'conditions': [
            ['OS=="win"', {
              'sources': [
                'win/nacl_plugin.def',
              ],
            }],
          ],
        },
        {
          'target_name': 'ppGoogleNaClPlugin',
          'type': 'shared_library',
          'variables': {
            'target_base': 'ppNaClPlugin',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio',
            '<(DEPTH)/native_client/src/shared/imc/imc.gyp:google_nacl_imc_c',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
            '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc',
            '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
            '<(DEPTH)/native_client/src/trusted/nonnacl_util/nonnacl_util.gyp:nonnacl_util',
            '<(DEPTH)/native_client/src/trusted/platform_qualify/platform_qualify.gyp:platform_qual_lib',
            '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:expiration',
            '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:gio_wrapped_desc',
            '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc',
            '<(DEPTH)/native_client/src/shared/ppapi/ppapi.gyp:ppapi_cpp',
            '<(DEPTH)/native_client/src/shared/ppapi_proxy/ppapi_proxy.gyp:nacl_ppapi_browser',
          ],
# TODO(noelallen) We will need to put this back in with a new .def file once we need to export symbols
# to support the plugin as a sandboxed DLL.
#          'conditions': [
#            ['OS=="win"', {
#              'sources': [
#                'win/nacl_plugin.def',
#              ],
#            }],
#          ],
        },
      ],
    }],
    ['OS=="win"', {
      'targets': [
        # ---------------------------------------------------------------------
        {
          'target_name': 'npGoogleNaClPlugin64',
          'type': 'shared_library',
          'variables': {
            'target_base': 'npNaClPlugin',
            'win_target': 'x64',
          },
          'sources': [
            'win/nacl_plugin64.def',
          ],
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio64',
            '<(DEPTH)/native_client/src/shared/imc/imc.gyp:google_nacl_imc_c64',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
            '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc64',
            '<(DEPTH)/native_client/src/shared/npruntime/npruntime.gyp:google_nacl_npruntime64',
            '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer64',
            '<(DEPTH)/native_client/src/trusted/nonnacl_util/nonnacl_util.gyp:nonnacl_util64',
            '<(DEPTH)/native_client/src/trusted/platform_qualify/platform_qualify.gyp:platform_qual_lib64',
            '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:expiration64',
            '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:gio_wrapped_desc64',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
    ['nacl_standalone==0', {
      'targets': [
        {
          # Static library for linking with Chrome.
          'target_name': 'npGoogleNaClPluginChrome',
          'type': 'static_library',
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio',
            '<(DEPTH)/native_client/src/shared/imc/imc.gyp:google_nacl_imc_c',
            '<(DEPTH)/native_client/src/shared/npruntime/npruntime.gyp:google_nacl_npruntime',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
            '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
            '<(DEPTH)/native_client/src/trusted/nonnacl_util/nonnacl_util.gyp:nonnacl_util_chrome',
            '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:expiration',
            '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:gio_wrapped_desc',
            '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
          ],
          'sources': [
            '<@(common_sources)',
            '<@(npapi_sources)',
            'nacl_entry_points.cc',
            'npapi/video_chrome.cc',
          ],
        },
      ],
    }],
    ['nacl_standalone==0 and OS=="win"', {
      'targets': [
        {
          'target_name': 'npGoogleNaClPluginChrome64',
          'type': 'static_library',
          'variables': {
            'win_target': 'x64',
          },
          'sources': [
            '<@(common_sources)',
            '<@(npapi_sources)',
            'nacl_entry_points.cc',
            'npapi/video_chrome.cc',
          ],
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio64',
            '<(DEPTH)/native_client/src/shared/imc/imc.gyp:google_nacl_imc_c64',
            '<(DEPTH)/native_client/src/shared/npruntime/npruntime.gyp:google_nacl_npruntime64',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
            '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer64',
            '<(DEPTH)/native_client/src/trusted/nonnacl_util/nonnacl_util.gyp:nonnacl_util_chrome64',
            '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:expiration64',
            '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:gio_wrapped_desc64',
            '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        }
      ],
    }],
  ],
}
