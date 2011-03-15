# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# PLEASE NOTE: This file contains the targets for generating the
# plugin two different ways -- once as a shared library, and once as a
# static library.  The static library is only built if we are inside
# of a Chrome tree, and it gets built with different defined symbols,
# and without the packaging code on the Mac.  The shared library gets
# built in an o3d tree, or in a chrome tree when built by an o3d
# developer (someone who has added the .gclient stanza to include
# DEPS_chrome).  This results in having two targets in this file which
# are largely identical, but still significantly different.
#
# Please be sure and synchronize these two targets so that we can
# continue to build both the standalone plugin and the Chrome-embedded
# plugin.

{
  'variables': {
    'chromium_code': 1,
    'plugin_sources': [
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
      'cross/whitelist.cc',
    ],
    'plugin_depends': [
      '../../<(jpegdir)/libjpeg.gyp:libjpeg',
      '../../<(pngdir)/libpng.gyp:libpng',
      '../../<(zlibdir)/zlib.gyp:zlib',
      '../../base/base.gyp:base',
      '../../skia/skia.gyp:skia',
      '../../v8/tools/gyp/v8.gyp:v8',
      '../core/core.gyp:o3dCore',
      '../core/core.gyp:o3dCorePlatform',
      '../import/archive.gyp:o3dArchive',
      '../utils/utils.gyp:o3dUtils',
      '../../native_client/src/shared/imc/imc.gyp:google_nacl_imc',
      'idl/idl.gyp:o3dPluginIdl',
    ],
    # A comma-separated list of strings, each double-quoted.
    'plugin_domain_whitelist%': '',
  },
  'includes': [
    '../build/branding.gypi',
    '../build/common.gypi',
    '../build/version.gypi',
  ],
  'target_defaults': {
    'include_dirs': [
      '..',
      '../..',
      '../../<(gtestdir)',
    ],
    'defines': [
      'O3D_PLUGIN_DESCRIPTION="<!(python version_info.py --set_name="<(plugin_name)" --set_version="<(plugin_version)" --set_npapi_mimetype="<(plugin_npapi_mimetype)" --description)"',
      'O3D_PLUGIN_NPAPI_FILENAME="<(plugin_npapi_filename)"',
      'O3D_PLUGIN_NPAPI_MIMETYPE="<(plugin_npapi_mimetype)"',
      'O3D_PLUGIN_NAME="<(plugin_name)"',
      'O3D_PLUGIN_VERSION="<(plugin_version)"',
      'O3D_PLUGIN_INSTALLDIR_CSIDL=<(plugin_installdir_csidl)',
      'O3D_PLUGIN_VENDOR_DIRECTORY="<(plugin_vendor_directory)"',
      'O3D_PLUGIN_PRODUCT_DIRECTORY="<(plugin_product_directory)"',
    ],
    'conditions': [
      # The funky quoting here is so that GYP doesn't shoot itself in the foot
      # when expanding a quoted variable which itself contains quotes.
      ["""'<(plugin_domain_whitelist)' != ''""",
        {
          'defines': [
            'O3D_PLUGIN_DOMAIN_WHITELIST=<(plugin_domain_whitelist)',
          ],
        },
      ],
    ],
  },
  'targets': [
    {
      # This is the shared library version of the plugin.
      'variables': {
        # Default values. Can be overridden with GYP_DEFINES for ease of
        # repackaging.
        'plugin_rpath%'         : '/opt/google/o3d/lib',      # empty => none
        'plugin_env_vars_file%' : '/opt/google/o3d/envvars',  # empty => none
        'conditions' : [
          ['renderer == "gl"',
            {
              'as_needed_ldflags': [
                # The Cg libs use three other libraries without linking to them,
                # which breaks --as-needed, so we have to specify them here before
                # the --as-needed flag.
                '-lGL',       # Used by libCgGL
                '-lpthread',  # Used by libCg
                '-lm',        # Used by libCg
                # GYP dumps all static and shared libraries into one archive group
                # on the command line in arbitrary order, which breaks
                # --as-needed, so we have to specify the out-of-order ones before
                # the --as-needed flag.
                '-lCgGL',
                '-ldl',      # Used by breakpad
                '-lrt',
              ]
            }, {
              'as_needed_ldflags': []
            }
          ],
        ]
      },
      'target_name': '<(plugin_npapi_filename)',
      'type': 'loadable_module',
      'dependencies': [
        '<@(plugin_depends)',
        'idl/idl.gyp:o3dNpnApi',
      ],
      'sources': [
        '<@(plugin_sources)',
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
        ['renderer == "gl"',
          {
            'dependencies': [
              '../build/libs.gyp:gl_libs',
              '../build/libs.gyp:cg_libs',
            ],
          },
        ],
        ['renderer == "gles2"',
          {
            'dependencies': [
              '../build/libs.gyp:gles2_libs',
            ],
          },
        ],
        ['OS == "mac"',
          {
            'mac_bundle': 1,
            'product_extension': 'plugin',
            'conditions': [
              ['"<(plugin_npapi_filename)" == "npo3dautoplugin"',
                {
                  # The unbranded Mac plugin's name is a special case.
                  'product_name': 'O3D',
                },
                {
                  'product_name': '<(plugin_npapi_filename)',
                },
              ],
            ],
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
              'mac/fullscreen_window_mac.h',
              'mac/fullscreen_window_mac.mm',
              'mac/o3d_layer.mm',
              'mac/o3d_plugin.r',
              'mac/overlay_window_mac.h',
              'mac/overlay_window_mac.mm',
              'mac/plugin_logging-mac.mm',
              'mac/plugin_mac.h',
              'mac/plugin_mac.mm',
              'mac/graphics_utils_mac.mm',
              'mac/main_mac.mm',
            ],
            'mac_framework_dirs': [
              '../../<(cgdir)',
            ],
            'include_dirs': [
              '../../breakpad/src/client/mac/Framework',
            ],
            'defines': [
              'XP_MACOSX=1',
            ],
            'link_settings': {
              'libraries': [
                '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
                '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
                '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
                '$(SDKROOT)/System/Library/Frameworks/AGL.framework',
                '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
                '$(SDKROOT)/System/Library/Frameworks/IOKit.framework',
                '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
                '$(SDKROOT)/System/Library/Frameworks/QuickTime.framework',
                'libbreakpad.a',
                'libbreakpad_utilities.a',
                '../../<(glewdir)/lib/libMacStaticGLEW.a',
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
                'conditions': [
                  ['"<(plugin_npapi_filename)" == "npo3dautoplugin"',
                    {
                      # The unbranded Mac plugin's name is a special case.
                      'action': ['<(copy_frameworks_path)', 'O3D'],
                    },
                    {
                      'action': ['<(copy_frameworks_path)',
                                 '<(plugin_npapi_filename)'],
                    },
                  ],
                ],
              },
              {
                'postbuild_name': 'Process Resource File',
                'action': ['python',
                  'version_info.py',
                  '--set_name=<(plugin_name)',
                  '--set_version=<(plugin_version)',
                  '--set_npapi_mimetype=<(plugin_npapi_mimetype)',
                  'mac/o3d_plugin.r',
                  '${BUILT_PRODUCTS_DIR}/O3D.r',
                ],
              },
              {
                'postbuild_name': 'Compile Resource File',
                'conditions': [
                  ['"<(plugin_npapi_filename)" == "npo3dautoplugin"',
                    {
                      # The unbranded Mac plugin's name is a special case.
                      'action': ['/usr/bin/Rez',
                        '-useDF',
                        '-o',
                        '${BUILT_PRODUCTS_DIR}/O3D.plugin/Contents/Resources/O3D.rsrc',
                        '${BUILT_PRODUCTS_DIR}/O3D.r',
                      ],
                    },
                    {
                      'action': ['/usr/bin/Rez',
                        '-useDF',
                        '-o',
                        '${BUILT_PRODUCTS_DIR}/<(plugin_npapi_filename).plugin/Contents/Resources/<(plugin_npapi_filename).rsrc',
                        '${BUILT_PRODUCTS_DIR}/O3D.r',
                      ],
                    },
                  ],
                ],
              },
            ],
          },
        ],
        ['OS == "linux"',
          {
            'dependencies': [
              '../../breakpad/breakpad.gyp:breakpad_client',
              '../breakpad/breakpad.gyp:o3dBreakpad',
            ],
            'sources': [
              'linux/config.cc',
              'linux/envvars.cc',
              'linux/main_linux.cc',
            ],
            'ldflags': [
              '-Wl,-znodelete',
              '-Wl,--gc-sections',
              '<!@(pkg-config --libs-only-L xt)',
              '<(as_needed_ldflags)',
              # Directs the linker to only generate dependencies on libraries
              # that we actually use. Must come last.
              '-Wl,--as-needed',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l xt)',
            ],
            'conditions' : [
              ['plugin_rpath != ""',
                {
                  'ldflags': [
                    '-Wl,-rpath', '-Wl,<(plugin_rpath)',
                  ],
                },
              ],
              ['plugin_env_vars_file != ""',
                {
                  'defines': [
                    'O3D_PLUGIN_ENV_VARS_FILE="<(plugin_env_vars_file)"',
                  ],
                },
              ],
            ],
          },
        ],
        ['OS == "win"',
          {
            'dependencies': [
              '../breakpad/breakpad.gyp:o3dBreakpad',
            ],
            'include_dirs': [
              # So that o3dPlugin.rc can find resource.h.
              'win',
            ],
            'sources': [
              'win/config.cc',
              'win/logger_main.cc',
              'win/main_win.cc',
              '<(SHARED_INTERMEDIATE_DIR)/plugin/o3dPlugin.def',
              '<(SHARED_INTERMEDIATE_DIR)/plugin/o3dPlugin.rc',
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
                '"$(DXSDK_DIR)/Lib/x86/d3dx9.lib"',
                '-ld3d9.lib',
                '"$(DXSDK_DIR)/Lib/x86/DxErr.lib"',
              ],
            },
          },
        ],
      ],
    },
  ],
  'conditions': [
    ['o3d_in_chrome == "True"',
      {
        # Only use the "static_library" plugin target if we're
        # building in a chrome tree, since we don't need it in an O3D
        # tree.
        'targets': [
          {
            'variables': {
              # By default the built-in Chrome version does not read an env
              # vars file, but this can be overridden by giving a different
              # value for this.
              'plugin_env_vars_file%' : '',
            },
            'target_name': 'o3dPlugin',
            'type': 'static_library',
            'dependencies': [
              '<@(plugin_depends)',
            ],
            'sources': [
              '<@(plugin_sources)',
            ],
            'defines':['O3D_INTERNAL_PLUGIN=1'],
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
              ['renderer == "gl"',
                {
                  'dependencies': [
                    '../build/libs.gyp:gl_libs',
                    '../build/libs.gyp:cg_libs',
                  ],
                },
              ],
              ['renderer == "gles2"',
                {
                  'dependencies': [
                    '../build/libs.gyp:gles2_libs',
                  ],
                },
              ],
              ['OS == "mac"',
                {
                  'mac_bundle': 1,
                  'product_extension': 'plugin',
                  'conditions': [
                    ['"<(plugin_npapi_filename)" == "npo3dautoplugin"',
                      {
                        # The unbranded Mac plugin's name is a special case.
                        'product_name': 'O3D',
                      },
                      {
                        'product_name': '<(plugin_npapi_filename)',
                      },
                    ],
                  ],
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
                    '../../<(cgdir)',
                  ],
                  'include_dirs': [
                    '../../breakpad/src/client/mac/Framework',
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
                      'libbreakpad.a',
                      'libbreakpad_utilities.a',
                      '../../<(glewdir)/lib/libMacStaticGLEW.a',
                    ],
                  },
                },
              ],
              ['OS == "linux"',
                {
                  'sources': [
                    'linux/main_linux.cc',
                    'linux/config.cc',
                    'linux/envvars.cc',
                  ],
                  'link_settings': {
                    'libraries': [
                      '-lGL',
                    ],
                  },
                  # On Linux, shared library targets aren't copied to the
                  # product dir automatically.  Filed GYP issue #74 to address this.
                  # TODO(gspencer): Remove when issue #74 is resolved.
                  'copies': [
                    {
                      'destination': '<(PRODUCT_DIR)',
                      'files': [
                        '<(PRODUCT_DIR)/obj/o3d/plugin/<(SHARED_LIB_PREFIX)<(_target_name)<(SHARED_LIB_SUFFIX)',
                      ],
                    },
                  ],
                  'conditions' : [
                    ['plugin_env_vars_file != ""',
                      {
                        'defines': [
                          'O3D_PLUGIN_ENV_VARS_FILE="<(plugin_env_vars_file)"',
                        ],
                      },
                    ],
                  ],
                },
              ],
              ['OS == "win"',
                {
                  'dependencies': [
                    '../breakpad/breakpad.gyp:o3dBreakpad',
                  ],
                  'include_dirs': [
                    # So that o3dPlugin.rc can find resource.h.
                    'win',
                  ],
                  'sources': [
                    'win/config.cc',
                    'win/logger_main.cc',
                    'win/main_win.cc',
                    '<(SHARED_INTERMEDIATE_DIR)/plugin/o3dPlugin.def',
                    '<(SHARED_INTERMEDIATE_DIR)/plugin/o3dPlugin.rc',
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
                      '"$(DXSDK_DIR)/Lib/x86/d3dx9.lib"',
                      '-ld3d9.lib',
                    ],
                  },
                },
              ],
              ['OS == "win" and renderer == "d3d9"',
                {
                  'link_settings': {
                    'libraries': [
                      '"$(DXSDK_DIR)/Lib/x86/DxErr.lib"',
                    ],
                  },
                },
              ],
            ],
          },
        ],
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
                'action_name': 'add_version_rc',
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
                        '<(SHARED_INTERMEDIATE_DIR)/plugin/o3dPlugin.rc'
                      ],
                      'action': ['python',
                        'version_info.py',
                        '--set_name=<(plugin_name)',
                        '--set_version=<(plugin_version)',
                        '--set_npapi_filename=<(plugin_npapi_filename)',
                        '--set_npapi_mimetype=<(plugin_npapi_mimetype)',
                        'win/o3dPlugin.rc_template',
                        '<(SHARED_INTERMEDIATE_DIR)/plugin/o3dPlugin.rc'],
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
                      'variables': {
                        # Ridiculously, it is impossible to define a GYP 
                        # variable whose value depends on the choice of build
                        # configuration, so to get this to be the staging URL
                        # when building Debug we define it as a shell fragment
                        # that uses command substitution to evaluate to
                        # different things based on the Xcode "CONFIGURATION" 
                        # environment variable. We further have to use ``
                        # instead of $() because GYP/Xcode mangles $().
                        'o3d_plugin_breakpad_url':
                          '`if [ "$CONFIGURATION" = Debug ]; then '
                            'echo https://clients2.google.com/cr/staging_report; '
                          'else '
                            'echo https://clients2.google.com/cr/report; '
                          'fi`'
                      },
                      'conditions': [
                        ['"<(plugin_npapi_filename)" == "npo3dautoplugin"',
                          {
                            # The unbranded Mac plugin's name is a special case.
                            'action': ['sh',
                              '-c',
                              'python '
                              'version_info.py '
                              '--set_name="<(plugin_name)" '
                              '--set_version="<(plugin_version)" '
                              '--set_npapi_filename="O3D" '
                              '--set_npapi_mimetype="<(plugin_npapi_mimetype)" '
                              '--set_o3d_plugin_breakpad_url="<(o3d_plugin_breakpad_url)" '
                              '"mac/Info.plist" '
                              '"<(SHARED_INTERMEDIATE_DIR)/plugin/Info.plist"',
                            ],
                          },
                          {
                            'action': ['sh',
                              '-c',
                              'python '
                              'version_info.py '
                              '--set_name="<(plugin_name)" '
                              '--set_version="<(plugin_version)" '
                              '--set_npapi_filename="<(plugin_npapi_filename)" '
                              '--set_npapi_mimetype="<(plugin_npapi_mimetype)" '
                              '--set_o3d_plugin_breakpad_url="<(o3d_plugin_breakpad_url)" ' 
                              '"mac/Info.plist" '
                              '"<(SHARED_INTERMEDIATE_DIR)/plugin/Info.plist"',
                            ],
                          },
                        ],
                      ],
                    },
                  ],
                ],
              },
            ],
            'conditions': [
              ['OS=="win"',
                {
                  'actions': [
                    {
                      'action_name': 'add_version_def',
                      'inputs': [
                        'version_info.py',
                        'win/o3dPlugin.def_template',
                      ],
                      'outputs': [
                        '<(SHARED_INTERMEDIATE_DIR)/plugin/o3dPlugin.def',
                      ],
                      'action': ['python',
                        'version_info.py',
                        '--set_npapi_filename=<(plugin_npapi_filename)',
                        'win/o3dPlugin.def_template',
                        '<(SHARED_INTERMEDIATE_DIR)/plugin/o3dPlugin.def'],
                    },
                  ],
                },
              ],
            ],
          },
        ],
      },
    ],
    ['OS=="win"',
      {
        'targets': [
          {
            'target_name': 'gen_host_control_rgs',
            'type': 'none',
            'actions': [
              {
                'action_name': 'gen_host_control_rgs',
                'inputs': [
                  'version_info.py',
                  'npapi_host_control/win/host_control.rgs_template',
                ],
                'outputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/plugin/host_control.rgs',
                ],
                'action': ['python',
                  'version_info.py',
                  '--set_version=<(plugin_version)',
                  '--set_activex_hostcontrol_clsid=' +
                      '<(plugin_activex_hostcontrol_clsid)',
                  '--set_activex_typelib_clsid=<(plugin_activex_typelib_clsid)',
                  '--set_activex_hostcontrol_name=' +
                      '<(plugin_activex_hostcontrol_name)',
                  '--set_activex_typelib_name=<(plugin_activex_typelib_name)',
                  'npapi_host_control/win/host_control.rgs_template',
                  '<(SHARED_INTERMEDIATE_DIR)/plugin/host_control.rgs',
                ],
              },
            ],
          },
          {
            'target_name': 'gen_npapi_host_control_idl',
            'type': 'none',
            'actions': [
              {
                'action_name': 'gen_npapi_host_control_idl',
                'inputs': [
                  'version_info.py',
                  'npapi_host_control/win/npapi_host_control.idl_template',
                ],
                'outputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/plugin/npapi_host_control.idl',
                ],
                'action': ['python',
                  'version_info.py',
                  '--set_version=<(plugin_version)',
                  '--set_activex_hostcontrol_clsid=' +
                      '<(plugin_activex_hostcontrol_clsid)',
                  '--set_activex_typelib_clsid=<(plugin_activex_typelib_clsid)',
                  '--set_activex_hostcontrol_name=' +
                      '<(plugin_activex_hostcontrol_name)',
                  '--set_activex_typelib_name=<(plugin_activex_typelib_name)',
                  'npapi_host_control/win/npapi_host_control.idl_template',
                  '<(SHARED_INTERMEDIATE_DIR)/plugin/npapi_host_control.idl',
                ],
              },
            ],
          },
          {
            'target_name': 'o3d_host',
            'type': 'shared_library',
            'dependencies': [
              'gen_host_control_rgs',
              'gen_npapi_host_control_idl',
            ],
            'include_dirs': [
              '<(INTERMEDIATE_DIR)',
              # So that npapi_host_control.rc can find host_control.rgs.
              '<(SHARED_INTERMEDIATE_DIR)/plugin',
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
              '<(SHARED_INTERMEDIATE_DIR)/plugin/npapi_host_control.idl',
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
    # If compiling with re-branding, we alias the branded NPAPI target name to
    # the unbranded one so that targets depending on it can just refer to it
    # by a constant name.
    ['"<(plugin_npapi_filename)" != "npo3dautoplugin"', 
      {
        'targets': [
          {
            'target_name': 'npo3dautoplugin',
            'type': 'none',
            'dependencies': [
              '<(plugin_npapi_filename)',
            ],
          },
        ],
      },
    ],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
