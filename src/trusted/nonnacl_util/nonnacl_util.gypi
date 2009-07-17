# Copyright 2009, Google Inc.
# All rights reserved.
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

{
  'variables': {
    'common_sources': [
      'sel_ldr_launcher.cc',
      'sel_ldr_launcher.h',
    ],
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'conditions': [
      ['OS=="linux"', {
        'defines': [
          'XP_UNIX',
        ],
        'sources': [
          'linux/sel_ldr_launcher_linux.cc',
          'linux/get_plugin_dirname.cc',
        ],
      }],
      ['OS=="mac"', {
        'defines': [
          'XP_MACOSX',
          'XP_UNIX',
          ['TARGET_API_MAC_CARBON', '1'],
          'NO_X11',
          'USE_SYSTEM_CONSOLE',
        ],
        'sources': [
          'linux/sel_ldr_launcher_linux.cc',
          'osx/get_plugin_dirname.mm',
        ],
      }],
      ['OS=="win"', {
        'defines': [
          'XP_WIN',
          'WIN32',
          '_WINDOWS'
        ],
        'sources': [
          'win/sel_ldr_launcher_win.cc',
        ],
      }],
    ],
  },
}
