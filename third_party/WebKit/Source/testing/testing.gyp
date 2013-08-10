#
# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#         * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#         * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#         * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

{
    'variables': {
        'ahem_path': '../../Source/testing/data/fonts/AHEM____.TTF',
        'source_dir': '../../Source',
        'conditions': [
            ['OS=="linux"', {
                'use_custom_freetype%': 1,
            }, {
                'use_custom_freetype%': 0,
            }],
        ],
    },
    'includes': [
        '../../Source/core/features.gypi',
        '../../Source/testing/runner/runner.gypi',
        '../../Source/testing/plugin/plugin.gypi',
    ],
    'targets': [
        {
            'target_name': 'TestRunner',
            'type': '<(component)',
            'defines': [
                'WEBTESTRUNNER_IMPLEMENTATION=1',
            ],
            'dependencies': [
                'TestRunner_resources',
                '../../public/blink.gyp:blink',
                '<(source_dir)/web/web.gyp:webkit_test_support',
            ],
            'export_dependent_settings': [
                '../../public/blink.gyp:blink',
            ],
            'direct_dependent_settings': {
                'include_dirs': [
                    '../..',
                ],
            },
            'sources': [
                '<@(test_runner_files)',
            ],
            'conditions': [
                ['component=="shared_library"', {
                    'defines': [
                        'WEBTESTRUNNER_DLL',
                        'WEBTESTRUNNER_IMPLEMENTATION=1',
                    ],
                    'dependencies': [
                        '<(DEPTH)/base/base.gyp:base',
                        '<(DEPTH)/skia/skia.gyp:skia',
                        '<(DEPTH)/url/url.gyp:url_lib',
                        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
                    ],
                    'direct_dependent_settings': {
                        'defines': [
                            'WEBTESTRUNNER_DLL',
                        ],
                    },
                    'export_dependent_settings': [
                        '<(DEPTH)/url/url.gyp:url_lib',
                        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
                    ],
                    'msvs_settings': {
                        'VCLinkerTool': {
                            'conditions': [
                                ['incremental_chrome_dll==1', {
                                    'UseLibraryDependencyInputs': 'true',
                                }],
                            ],
                        },
                    },
                }],
                ['toolkit_uses_gtk == 1', {
                    'dependencies': [
                        '<(DEPTH)/build/linux/system.gyp:gtk',
                    ],
                    'include_dirs': [
                        '../../../public/web/gtk',
                    ],
                }],
                ['OS!="win"', {
                    'sources/': [
                        ['exclude', 'Win\\.cpp$'],
                    ],
                }],
            ],
            # Disable c4267 warnings until we fix size_t to int truncations.
            'msvs_disabled_warnings': [ 4267, ],
        },
        {
            # FIXME: This is only used by webkit_unit_tests now, move it to WebKitUnitTests.gyp.
            'target_name': 'DumpRenderTree_resources',
            'type': 'none',
            'dependencies': [
                '<(DEPTH)/net/net.gyp:net_resources',
                '<(DEPTH)/ui/ui.gyp:ui_resources',
                '<(DEPTH)/webkit/webkit_resources.gyp:webkit_resources',
                '<(DEPTH)/webkit/webkit_resources.gyp:webkit_strings',
            ],
            'actions': [{
                'action_name': 'repack_local',
                'variables': {
                    'repack_path': '<(DEPTH)/tools/grit/grit/format/repack.py',
                    'pak_inputs': [
                        '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
                        '<(SHARED_INTERMEDIATE_DIR)/ui/gfx/gfx_resources.pak',
                        '<(SHARED_INTERMEDIATE_DIR)/webkit/blink_resources.pak',
                        '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_strings_en-US.pak',
                        '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources_100_percent.pak',
                ]},
                'inputs': [
                    '<(repack_path)',
                    '<@(pak_inputs)',
                ],
                'outputs': [
                    '<(PRODUCT_DIR)/DumpRenderTree.pak',
                ],
                'action': ['python', '<(repack_path)', '<@(_outputs)', '<@(pak_inputs)'],
            }],
            'conditions': [
                ['OS=="mac"', {
                    'all_dependent_settings': {
                        'mac_bundle_resources': [
                            '<(PRODUCT_DIR)/DumpRenderTree.pak',
                        ],
                    },
                }],
            ]
        },
        {
            'target_name': 'TestRunner_resources',
            'type': 'none',
            'dependencies': [
                'copy_TestNetscapePlugIn',
            ],
            'conditions': [
                ['OS=="win"', {
                    'dependencies': [
                        'LayoutTestHelper',
                    ],
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)',
                        'files': ['<(ahem_path)'],
                    }],
                }],
                ['OS=="mac"', {
                    'dependencies': [
                        'LayoutTestHelper',
                    ],
                    'all_dependent_settings': {
                        'mac_bundle_resources': [
                            '<(ahem_path)',
                            '<(source_dir)/testing/data/fonts/WebKitWeightWatcher100.ttf',
                            '<(source_dir)/testing/data/fonts/WebKitWeightWatcher200.ttf',
                            '<(source_dir)/testing/data/fonts/WebKitWeightWatcher300.ttf',
                            '<(source_dir)/testing/data/fonts/WebKitWeightWatcher400.ttf',
                            '<(source_dir)/testing/data/fonts/WebKitWeightWatcher500.ttf',
                            '<(source_dir)/testing/data/fonts/WebKitWeightWatcher600.ttf',
                            '<(source_dir)/testing/data/fonts/WebKitWeightWatcher700.ttf',
                            '<(source_dir)/testing/data/fonts/WebKitWeightWatcher800.ttf',
                            '<(source_dir)/testing/data/fonts/WebKitWeightWatcher900.ttf',
                            '<(SHARED_INTERMEDIATE_DIR)/webkit/missingImage.png',
                            '<(SHARED_INTERMEDIATE_DIR)/webkit/textAreaResizeCorner.png',
                        ],
                    },
                }],
                # The test plugin relies on X11.
                ['OS=="linux" and use_x11==0', {
                    'dependencies!': [
                        'copy_TestNetscapePlugIn',
                    ],
                }],
                ['use_x11 == 1', {
                    'dependencies': [
                        '<(DEPTH)/tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
                    ],
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)',
                        'files': [
                            '<(ahem_path)',
                            '<(source_dir)/testing/data/fonts/fonts.conf',
                        ]
                    }],
                }],
                ['OS=="android"', {
                    'dependencies!': [
                        'copy_TestNetscapePlugIn',
                    ],
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)',
                        'files': [
                            '<(ahem_path)',
                            '<(source_dir)/testing/data/fonts/android_main_fonts.xml',
                            '<(source_dir)/testing/data/fonts/android_fallback_fonts.xml',
                        ]
                    }],
                }],
            ],
        },
        {
            'target_name': 'TestNetscapePlugIn',
            'type': 'loadable_module',
            'sources': [ '<@(test_plugin_files)' ],
            'dependencies': [
                '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
            ],
            'include_dirs': [
                '<(DEPTH)',
                '<(source_dir)/testing/plugin/',
            ],
            'conditions': [
                ['OS=="mac"', {
                    'mac_bundle': 1,
                    'product_extension': 'plugin',
                    'link_settings': {
                        'libraries': [
                            '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
                            '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
                            '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
                        ]
                    },
                    'xcode_settings': {
                        'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',
                        'INFOPLIST_FILE': 'plugin/mac/Info.plist',
                    },
                }],
                ['os_posix == 1 and OS != "mac"', {
                    'cflags': [
                        '-fvisibility=default',
                    ],
                }],
                ['OS=="win"', {
                    'defines': [
                        # This seems like a hack, but this is what Safari Win does.
                        'snprintf=_snprintf',
                    ],
                    'sources': [
                        'plugin/win/TestNetscapePlugin.def',
                        'plugin/win/TestNetscapePlugin.rc',
                    ],
                    # The .rc file requires that the name of the dll is npTestNetscapePlugIn.dll.
                    'product_name': 'npTestNetscapePlugIn',
                    # Disable c4267 warnings until we fix size_t to int truncations.
                    'msvs_disabled_warnings': [ 4267, ],
                }],
            ],
        },
        {
            'target_name': 'copy_TestNetscapePlugIn',
            'type': 'none',
            'dependencies': [
                'TestNetscapePlugIn',
            ],
            'conditions': [
                ['OS=="win"', {
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)/plugins',
                        'files': ['<(PRODUCT_DIR)/npTestNetscapePlugIn.dll'],
                    }],
                }],
                ['OS=="mac"', {
                    'dependencies': ['TestNetscapePlugIn'],
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)/plugins/',
                        'files': ['<(PRODUCT_DIR)/TestNetscapePlugIn.plugin/'],
                    }],
                }],
                ['os_posix == 1 and OS != "mac"', {
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)/plugins',
                        'files': ['<(PRODUCT_DIR)/libTestNetscapePlugIn.so'],
                    }],
                }],
            ],
        },
    ], # targets
    'conditions': [
        ['OS=="win"', {
            'targets': [{
                'target_name': 'LayoutTestHelper',
                'type': 'executable',
                'sources': ['helper/LayoutTestHelperWin.cpp'],
            }],
        }],
        ['OS=="mac"', {
            'targets': [{
                'target_name': 'LayoutTestHelper',
                'type': 'executable',
                'sources': ['helper/LayoutTestHelperMac.mm'],
                'link_settings': {
                    'libraries': [
                        '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
                    ],
                },
            }],
        }],
        ['gcc_version>=46', {
            'target_defaults': {
                # Disable warnings about c++0x compatibility, as some names (such
                # as nullptr) conflict with upcoming c++0x types.
                'cflags_cc': ['-Wno-c++0x-compat'],
            },
        }],
        ['clang==1', {
            'target_defaults': {
                # FIXME: Add -Wglobal-constructors after fixing existing bugs.
                'cflags': ['-Wunused-parameter'],
                'xcode_settings': {
                    'WARNING_CFLAGS': ['-Wunused-parameter'],
                },
            },
        }],
    ], # conditions
}
