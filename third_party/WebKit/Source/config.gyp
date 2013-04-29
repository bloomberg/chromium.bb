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
  'variables': {
    # If set to 1, doesn't compile debug symbols into webcore reducing the
    # size of the binary and increasing the speed of gdb.  gcc only.
    'remove_webcore_debug_symbols%': 0,
  },
  'targets': [{
    'target_name': 'config',
    'type': 'none',
    'direct_dependent_settings': {
      'defines': [
        'WEBKIT_IMPLEMENTATION=1',
      ],
      'msvs_disabled_warnings': [
        4138, 4244, 4291, 4305, 4344, 4355, 4521, 4099,
      ],
      'conditions': [
        ['OS=="win" and component=="shared_library"', {
          'defines': [
            'USING_V8_SHARED',
          ],
        }],
        ['OS=="win"', {
          'defines': [
            '__PRETTY_FUNCTION__=__FUNCTION__',
          ],
        }],
        ['OS!="win" and remove_webcore_debug_symbols==1', {
          # Remove -g from all targets defined here.
          'cflags!': ['-g'],
        }],
        ['gcc_version>=46', {
          # Disable warnings about c++0x compatibility, as some names (such as
          # nullptr) conflict with upcoming c++0x types.
          'cflags_cc': ['-Wno-c++0x-compat'],
        }],
        ['OS=="linux" and target_arch=="arm"', {
          # Due to a bug in gcc arm, we get warnings about uninitialized
          # timesNewRoman.unstatic.3258 and colorTransparent.unstatic.4879.
          'cflags': ['-Wno-uninitialized'],
        }],
        ['clang==1', {
          'cflags': ['-Wglobal-constructors'],
          'xcode_settings': {
            'WARNING_CFLAGS': ['-Wglobal-constructors'],
          },
        }],
      ],
    },
  }],
}
