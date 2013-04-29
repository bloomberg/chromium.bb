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
        '../../WebKit/chromium/WinPrecompile.gypi',
        '../Platform.gypi',
    ],
    'targets': [
        {
            'target_name': 'webkit_platform',
            'type': 'static_library',
            'dependencies': [
                '../../wtf/wtf.gyp:wtf',
                '<(DEPTH)/skia/skia.gyp:skia',
            ],
            'include_dirs': [
                '../chromium',
                '<(output_dir)',
            ],
            'defines': [
                'WEBKIT_IMPLEMENTATION=1',
            ],
            'sources': [
                '<@(platform_files)',
            ],
            'variables': {
                # List of headers that are #included in Platform API headers that exist inside
                # the WebCore directory. These are only included when WEBKIT_IMPLEMENTATION=1.
                # Since Platform/ can't add core/* to the include path, this build step
                # copies these headers into the shared intermediate directory and adds that to the include path.
                # This is temporary, the better solution is to move these headers into the Platform
                # directory for all ports and just use them as normal.
                'webcore_headers': [
                    '../../core/platform/graphics/FloatPoint.h',
                    '../../core/platform/graphics/FloatPoint3D.h',
                    '../../core/platform/graphics/FloatQuad.h',
                    '../../core/platform/graphics/FloatRect.h',
                    '../../core/platform/graphics/FloatSize.h',
                    '../../core/platform/graphics/IntPoint.h',
                    '../../core/platform/graphics/IntRect.h',
                    '../../core/platform/graphics/IntSize.h',
                ],
                'output_dir': '<(SHARED_INTERMEDIATE_DIR)/webcore_headers'
            },
            'direct_dependent_settings': {
                'include_dirs': [
                    '../chromium',
                    '<(output_dir)'
                ],
            },
            'conditions': [
                ['component=="shared_library"', {
                    'defines': [
                        'WEBKIT_DLL',
                    ],
                }],
                ['OS=="win"', {
                    # Disable c4267 warnings until we fix size_t to int truncations.
                    'msvs_disabled_warnings': [4267, ],
                }],
            ],
            'copies': [
                {
                    'destination': '<(output_dir)',
                    'files': [
                        '<@(webcore_headers)'
                    ]
                }
            ]
        }
    ]
}

