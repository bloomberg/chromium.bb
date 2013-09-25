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
    'includes': [
        '../bindings/bindings.gypi',
        '../core/core.gypi',
        '../core/features.gypi',
        '../modules/modules.gypi',
        '../web/web.gypi',
        '../wtf/wtf.gypi',
    ],
    'targets': [
        {
            'target_name': 'webkit_unit_tests',
            'type': 'executable',
            'variables': { 'enable_wexit_time_destructors': 1, },
            'msvs_guid': '7CEFE800-8403-418A-AD6A-2D52C6FC3EAD',
            'dependencies': [
                '../../public/blink.gyp:blink',
                '../testing/testing.gyp:DumpRenderTree_resources',
                '<(DEPTH)/base/base.gyp:base',
                '<(DEPTH)/base/base.gyp:base_i18n',
                '<(DEPTH)/base/base.gyp:test_support_base',
                '<(DEPTH)/testing/gmock.gyp:gmock',
                '<(DEPTH)/testing/gtest.gyp:gtest',
                '<(DEPTH)/third_party/libwebp/libwebp.gyp:libwebp',
                '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
                '<(DEPTH)/url/url.gyp:url_lib',
                '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
                '<(DEPTH)/content/content.gyp:content_webkit_unit_test_support',
            ],
            'sources': [
                '../web/tests/RunAllTests.cpp',
            ],
            'include_dirs': [
                '../../public/web',
                '../web',
                'src',
            ],
            'conditions': [
                ['component=="shared_library"', {
                    'defines': [
                        'WEBKIT_DLL_UNITTEST',
                    ],
                }, {
                    'dependencies': [
                        '../core/core.gyp:webcore',
                    ],
                    'defines': [
                        'BLINK_IMPLEMENTATION=1',
                        'INSIDE_WEBKIT',
                    ],
                    'sources': [
                        '<@(bindings_unittest_files)',
                        '<@(core_unittest_files)',
                        '<@(modules_unittest_files)',
                        '<@(web_unittest_files)',
                    ],
                    'conditions': [
                        ['toolkit_uses_gtk == 1', {
                            'include_dirs': [
                                '../../public/web/gtk',
                            ],
                            'variables': {
                            # FIXME: Enable warnings on other platforms.
                            'chromium_code': 1,
                            },
                        }],
                    ],
                }],
                ['OS=="win" and component!="shared_library"', {
                    'configurations': {
                        'Debug_Base': {
                            'msvs_settings': {
                                'VCLinkerTool': {
                                    'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                                },
                            },
                        },
                    },
                }],
                ['OS=="android" and gtest_target_type == "shared_library"', {
                    'type': 'shared_library',
                    'dependencies': [
                        '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
                        '<(DEPTH)/tools/android/forwarder2/forwarder.gyp:forwarder2',
                    ],
                }],
                ['OS=="mac"', {
                    'include_dirs': [
                        '../../public/web/mac',
                    ],
                }],
                [ 'os_posix==1 and OS!="mac" and OS!="android" and OS!="ios" and linux_use_tcmalloc==1', {
                    'dependencies': [
                        '<(DEPTH)/base/allocator/allocator.gyp:allocator',
                    ],
                }],
            ],
        }
    ], # targets
    'conditions': [
        ['gcc_version>=46', {
            'target_defaults': {
                # Disable warnings about c++0x compatibility, as some names (such
                # as nullptr) conflict with upcoming c++0x types.
                'cflags_cc': ['-Wno-c++0x-compat'],
            },
        }],
        ['OS=="android" and android_webview_build==0 and gtest_target_type == "shared_library"', {
            # Wrap libwebkit_unit_tests.so into an android apk for execution.
            'targets': [{
                'target_name': 'webkit_unit_tests_apk',
                'type': 'none',
                'dependencies': [
                    '<(DEPTH)/base/base.gyp:base_java',
                    '<(DEPTH)/net/net.gyp:net_java',
                    'webkit_unit_tests',
                ],
                'variables': {
                    'test_suite_name': 'webkit_unit_tests',
                    'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)webkit_unit_tests<(SHARED_LIB_SUFFIX)',
                },
                'includes': [ '../../../../build/apk_test.gypi' ],
            }],
        }],
        ['clang==1', {
            'target_defaults': {
                'cflags': ['-Wunused-parameter'],
                'xcode_settings': {
                    'WARNING_CFLAGS': ['-Wunused-parameter'],
                },
            },
        }],
    ],
}
