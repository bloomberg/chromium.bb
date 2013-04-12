#
# Copyright (C) 2012 Google Inc. All rights reserved.
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
        'tools_dir': '../..',
        'source_dir': '../../../Source',
    },
    'includes': [
        '../TestWebKitAPI.gypi',
        '../../../Source/WebKit/chromium/features.gypi',
    ],
    'targets': [
        {
            'target_name': 'TestWebKitAPI',
            'type': 'executable',
            'dependencies': [
                '<(source_dir)/WebCore/WebCore.gyp/WebCore.gyp:webcore',
                '<(source_dir)/bindings/bindings.gyp:bindings',
                '<(source_dir)/WebKit/chromium/WebKit.gyp:webkit',
                '<(source_dir)/WTF/WTF.gyp/WTF.gyp:wtf',
                '<(DEPTH)/base/base.gyp:test_support_base',
                '<(DEPTH)/testing/gmock.gyp:gmock',
                '<(DEPTH)/testing/gtest.gyp:gtest',
                '<(DEPTH)/url/url.gyp:url',
                '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
                '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_support',
            ],
            'include_dirs': [
                '<(tools_dir)/TestWebKitAPI',
                # Needed by tests/RunAllTests.cpp, as well as ChromiumCurrentTime.cpp and
                # ChromiumThreading.cpp in chromium shared library configuration.
                '<(source_dir)/WebKit/chromium/public',
                '<(tools_dir)/TestWebKitAPI/ForwardingHeaders',
            ],
            'sources': [
                # Reuse the same testing driver of Chromium's webkit_unit_tests.
                '<(source_dir)/WebKit/chromium/tests/RunAllTests.cpp',
                '<@(TestWebKitAPI_files)',
            ],
            'conditions': [
                ['component!="shared_library"', {
                    'sources': [
		        '../Tests/WebCore/HeapGraphSerializerTest.cpp'
                    ],
                }],
                ['component=="shared_library"', {
                    'sources': [
                        # To satisfy linking of WTF::currentTime() etc. in shared library configuration,
                        # as the symbols are not exported from the DLLs.
                        '<(source_dir)/WebKit/chromium/src/ChromiumCurrentTime.cpp',
                        '<(source_dir)/WebKit/chromium/src/ChromiumThreading.cpp',
                    ],
                }],
                ['OS=="android" and gtest_target_type == "shared_library"', {
                    'type': 'shared_library',
                    'dependencies': [
                        '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
                    ],
                }],
            ],
        },
    ], # targets
    'conditions': [
        ['OS=="android" and gtest_target_type == "shared_library"', {
            # Wrap libTestWebKitAPI.so into an android apk for execution.
            'targets': [{
                'target_name': 'TestWebKitAPI_apk',
                'type': 'none',
                'dependencies': [
                    '<(DEPTH)/base/base.gyp:base_java',
                    'TestWebKitAPI',
                ],
                'variables': {
                    'test_suite_name': 'TestWebKitAPI',
                    'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)TestWebKitAPI<(SHARED_LIB_SUFFIX)',
                },
                'includes': [ '../../../../../build/apk_test.gypi' ],
            }],
        }],
    ],
}
