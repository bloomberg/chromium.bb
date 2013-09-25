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
        '../Source/core/features.gypi',
    ],
    'targets': [
        {
            'target_name': 'all_blink',
            'type': 'none',
            'dependencies': [
                '../Source/testing/testing.gyp:TestRunner',
                '../Source/platform/platform_tests.gyp:platform_unittests',
                '../Source/web/web_tests.gyp:webkit_unit_tests',
                '../Source/weborigin/weborigin_tests.gyp:weborigin_unittests',
                '../Source/wtf/wtf_tests.gyp:wtf_unittests',
            ],
            'conditions': [
                # Special target to wrap a gtest_target_type==shared_library
                # webkit_unit_tests into an android apk for execution. See
                # base.gyp for TODO(jrg)s about this strategy.
                ['OS=="android" and android_webview_build==0 and gtest_target_type == "shared_library"', {
                    'dependencies': [
                        '../Source/web/web_tests.gyp:webkit_unit_tests_apk',
                    ],
                }],
            ],
        },
    ],
}
