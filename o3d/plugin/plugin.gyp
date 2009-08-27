# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/common.gypi',
  ],
  'target_defaults': {
    'include_dirs': [
      '..',
      '../..',
      '../../<(gtestdir)',
    ],
    'defines': [
      'O3D_PLUGIN_DESCRIPTION="<!(python version_info.py --description)"',
      'O3D_PLUGIN_MIME_TYPE="<!(python version_info.py --mimetype)"',
      'O3D_PLUGIN_NAME="<!(python version_info.py --name)"',
      'O3D_PLUGIN_VERSION="<!(python version_info.py --version)"',
    ],
  },
  'targets': [
    {
      'target_name': 'npo3dautoplugin',
      'type': '<(o3d_main_lib_type)',
      'dependencies': [
        '../../<(jpegdir)/libjpeg.gyp:libjpeg',
        '../../<(pngdir)/libpng.gyp:libpng',
        '../../<(zlibdir)/zlib.gyp:zlib',
        '../../base/base.gyp:base',
        '../../skia/skia.gyp:skia',
        '../../v8/tools/gyp/v8.gyp:v8',
        '../core/core.gyp:o3dCore',
        '../core/core.gyp:o3dCorePlatform',
        '../import/archive.gyp:o3dArchive',
        '../serializer/serializer.gyp:o3dSerializer',
        '../utils/utils.gyp:o3dUtils',
        '../../native_client/src/shared/imc/imc.gyp:google_nacl_imc',
        'idl/idl.gyp:o3dPluginIdl',
      ],
      'sources': [
        'cross/async_loading.cc',
        'cross/async_loading.h',
        'cross/blacklist.cc',
        'cross/config.h',
        'cross/config_common.cc',
        'cross/download_stream.h',
        'cross/main.cc',
        'cross/main.h',
        'cross/main_thread_task_poster.cc',
        'cross/main_thread_task_poster.h',
        'cross/marshaling_utils.h',
        'cross/np_v8_bridge.cc',
        'cross/np_v8_bridge.h',
        'cross/out_of_memory.cc',
        'cross/out_of_memory.h',
        'cross/plugin_logging.h',
        'cross/plugin_main.h',
        'cross/stream_manager.cc',
        'cross/stream_manager.h',
        'cross/texture_static_glue.cc',
      ],
      'conditions' : [
        ['OS != "linux"',
          {
            'dependencies': [
              '../statsreport/statsreport.gyp:o3dStatsReport',
              'add_version',
              'o3dPluginLogging',
            ],
          },
        ],
        ['OS == "mac"',
          {
            'mac_bundle': 1,
            'product_extension': 'plugin',
            'product_name': 'O3D',
            'dependencies': [
              '../../breakpad/breakpad.gyp:breakpad',
            ],
            'xcode_settings': {
             'INFOPLIST_FILE': '<(SHARED_INTERMEDIATE_DIR)/plugin/Info.plist',
            },
            'mac_bundle_resources': [
              'mac/Resources/English.lproj',
            ],
            'sources': [
              'mac/config_mac.mm',
              'mac/main_mac.mm',
              'mac/o3d_plugin.r',
              'mac/plugin_logging-mac.mm',
              'mac/plugin_mac.h',
              'mac/plugin_mac.mm',
              'mac/graphics_utils_mac.mm',
            ],
            'mac_framework_dirs': [
              '../../breakpad/src/client/mac/build/Release',
              '<(cgdir)',
            ],
            'defines': [
              'XP_MACOSX=1',
            ],
            'link_settings': {
              'libraries': [
                '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
                '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
                '$(SDKROOT)/System/Library/Frameworks/AGL.framework',
                '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
                '$(SDKROOT)/System/Library/Frameworks/IOKit.framework',
                '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
                '$(SDKROOT)/System/Library/Frameworks/QuickTime.framework',
                '../../breakpad/src/client/mac/build/Release/Breakpad.framework',
                '../../third_party/cg/files/mac/Cg.framework',
                '../../third_party/glew/files/lib/libMacStaticGLEW.a',
              ],
            },
            'postbuilds': [
              {
                'variables': {
                  # Define install_name in a variable ending in _path
                  # so that gyp understands it's a path and performs proper
                  # relativization during dict merging.
                  'install_name_path': 'mac/plugin_fix_install_names.sh',
                },
                'postbuild_name': 'Fix Framework Paths',
                'action': ['<(install_name_path)'],
              },
              {
                'variables': {
                  # Define copy_frameworks in a variable ending in _path
                  # so that gyp understands it's a path and performs proper
                  # relativization during dict merging.
                  'copy_frameworks_path': 'mac/plugin_copy_frameworks.sh',
                },
                'postbuild_name': 'Copy Frameworks',
                'action': ['<(copy_frameworks_path)'],
              },
              {
                'postbuild_name': 'Process Resource File',
                'action': ['python',
                           'version_info.py',
                           'mac/o3d_plugin.r',
                           '${BUILT_PRODUCTS_DIR}/O3D.r',
                           ],
              },
              {
                'postbuild_name': 'Compile Resource File',
                'action': ['/usr/bin/Rez',
                           '-o',
                           '${BUILT_PRODUCTS_DIR}/O3D.plugin/Contents/Resources/O3D.rsrc',
                           '${BUILT_PRODUCTS_DIR}/O3D.r',
                           ],
              },
            ],
          },
        ],
        ['OS == "win"',
          {
            'dependencies': [
              '../breakpad/breakpad.gyp:o3dBreakpad',
            ],
            'sources': [
              'win/config.cc',
              'win/logger_main.cc',
              'win/main_win.cc',
              'win/o3dPlugin.def',
              'win/o3dPlugin.rc',
              'win/plugin_logging-win32.cc',
              'win/resource.h',
              'win/update_lock.cc',
              'win/update_lock.h',
            ],
            'link_settings': {
              'libraries': [
                '-lrpcrt4.lib',
              ],
            },
          },
        ],
        ['OS == "win" and renderer == "d3d9"',
          {
            'link_settings': {
              'libraries': [
                '"$(DXSDK_DIR)/Lib/x86/DxErr9.lib"',
                '"$(DXSDK_DIR)/Lib/x86/d3dx9.lib"',
                '-ld3d9.lib',
              ],
            },
          },
        ],
      ],
    },
  ],
  'conditions': [
    ['o3d_in_chrome != 0',
      {
        'variables': {
          'o3d_main_lib_type': 'static_library',
        },
        'target_defaults': {
          'defines': [
            'O3D_INTERNAL_PLUGIN=1',
          ],
        },
      },
      {
        'variables': {
          'o3d_main_lib_type': 'loadable_module',
        },
      },
    ],
    ['OS != "linux"',
      {
        'targets': [
          {
            'target_name': 'o3dPluginLogging',
            'type': 'static_library',
            'conditions': [
              ['OS=="win"',
                {
                  'sources': [
                    'win/plugin_metrics-win32.cc',
                    'win/plugin_logging-win32.cc',
                  ],
                },
              ],
              ['OS=="mac"',
                {
                  'sources': [
                    'mac/plugin_metrics-mac.cc',
                    'mac/plugin_logging-mac.mm',
                  ],
                },
              ],
            ],
          },
          {
            'target_name': 'add_version',
            'type': 'none',
            'actions': [
              {
                'action_name': 'add_version',
                'inputs': [
                  'version_info.py',
                ],
                'conditions': [
                  ['OS=="win"',
                    {
                      'inputs': [
                        'win/o3dPlugin.rc_template',
                      ],
                      'outputs': [
                        'win/o3dPlugin.rc'
                      ],
                      'action': ['python',
                        'version_info.py',
                        'win/o3dPlugin.rc_template',
                        'win/o3dPlugin.rc'],
                    },
                  ],
                  ['OS=="mac"',
                    {
                      'inputs': [
                        'mac/Info.plist',
                      ],
                      'outputs': [
                        '<(SHARED_INTERMEDIATE_DIR)/plugin/Info.plist',
                      ],
                      'action': ['python',
                        'version_info.py',
                        'mac/Info.plist',
                        '<(SHARED_INTERMEDIATE_DIR)/plugin/Info.plist',
                      ],
                    },
                  ],
                ],
              },
            ],
          },
        ],
      },
    ],
    ['OS=="win"',
      {
        'targets': [
          {
            'target_name': 'o3d_host',
            'type': 'shared_library',
            'include_dirs': [
              '<(INTERMEDIATE_DIR)',
            ],
            'sources': [
              '<(INTERMEDIATE_DIR)/npapi_host_control_i.c',
              'npapi_host_control/win/dispatch_proxy.cc',
              'npapi_host_control/win/dispatch_proxy.h',
              'npapi_host_control/win/host_control.cc',
              'npapi_host_control/win/host_control.h',
              'npapi_host_control/win/module.h',
              'npapi_host_control/win/np_browser_proxy.cc',
              'npapi_host_control/win/np_browser_proxy.h',
              'npapi_host_control/win/np_object_proxy.cc',
              'npapi_host_control/win/np_object_proxy.h',
              'npapi_host_control/win/np_plugin_proxy.cc',
              'npapi_host_control/win/np_plugin_proxy.h',
              'npapi_host_control/win/npapi_host_control.cc',
              'npapi_host_control/win/npapi_host_control.idl',
              'npapi_host_control/win/npapi_host_control.rc',
              'npapi_host_control/win/precompile.h',
              'npapi_host_control/win/resource.h',
              'npapi_host_control/win/stream_operation.cc',
              'npapi_host_control/win/stream_operation.h',
              'npapi_host_control/win/variant_utils.cc',
              'npapi_host_control/win/variant_utils.h',
            ],
            'link_settings': {
              'libraries': [
                '-lwininet.lib',
              ],
            },
            'defines': [
              '_MIDL_USE_GUIDDEF_',
              'DLL_NPAPI_HOST_CONTROL_EXPORT',
            ],
            'msvs_settings': {
              'VCLinkerTool': {
                'ModuleDefinitionFile':
                  'npapi_host_control/win/npapi_host_control.def'
              },
              'VCCLCompilerTool': {
                'ForcedIncludeFiles':
                  'plugin/npapi_host_control/win/precompile.h',
                'CompileAs': '2', # Build all the files as C++, since
                                  # ATL requires that.
              },
            },
            'msvs_configuration_attributes': {
              'UseOfATL': '1', # 1 = static link to ATL, 2 = dynamic link
            },
          },
        ],
      },
    ],
  ],
}
