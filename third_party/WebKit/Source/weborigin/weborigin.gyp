#
# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
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
    '../build/win/precompile.gypi',
    'weborigin.gypi',
  ],
  'targets': [{
    'target_name': 'weborigin',
    'type': '<(component)',
    'dependencies': [
      '../config.gyp:config',
      '../wtf/wtf.gyp:wtf',
      '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
      '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
      '<(DEPTH)/url/url.gyp:url_lib',
    ],
    'export_dependent_settings': [
      '<(DEPTH)/url/url.gyp:url_lib',
    ],
    'defines': [
      'WEBORIGIN_IMPLEMENTATION=1',
    ],
    'sources': [
      '<@(weborigin_files)',
    ],
    # Disable c4267 warnings until we fix size_t to int truncations.
    'msvs_disabled_warnings': [ 4267, 4334, ]
  }],
}
