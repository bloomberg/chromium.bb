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
    '../build/features.gypi',
    '../build/win/precompile.gypi',
    'blink_heap.gypi',
  ],
  'targets': [
    {
      'target_name': 'blink_heap_asm_stubs',
      'type': 'static_library',
      # VS2010 does not correctly incrementally link obj files generated
      # from asm files. This flag disables UseLibraryDependencyInputs to
      # avoid this problem.
      'msvs_2010_disable_uldi_when_referenced': 1,
      'includes': [
        '../../../yasm/yasm_compile.gypi',
      ],
      'sources': [
        '<@(heap_asm_files)',
      ],
      'variables': {
        'more_yasm_flags': [],
        'conditions': [
          ['OS == "mac"', {
            'more_yasm_flags': [
              # Necessary to ensure symbols end up with a _ prefix; added by
              # yasm_compile.gypi for Windows, but not Mac.
              '-DPREFIX',
            ],
          }],
          ['OS == "win" and target_arch == "x64"', {
            'more_yasm_flags': [
              '-DX64WIN=1',
            ],
          }],
          ['OS != "win" and target_arch == "x64"', {
            'more_yasm_flags': [
              '-DX64POSIX=1',
            ],
          }],
          ['target_arch == "ia32"', {
            'more_yasm_flags': [
              '-DIA32=1',
            ],
          }],
          ['target_arch == "arm"', {
            'more_yasm_flags': [
              '-DARM=1',
            ],
          }],
        ],
        'yasm_flags': [
          '>@(more_yasm_flags)',
        ],
        'yasm_output_path': '<(SHARED_INTERMEDIATE_DIR)/webcore/heap'
      },
    },
    {
      'target_name': 'blink_heap',
      'type': '<(component)',
      'dependencies': [
        '../config.gyp:config',
        '../wtf/wtf.gyp:wtf',
        'blink_heap_asm_stubs',
      ],
      'defines': [
        'HEAP_IMPLEMENTATION=1',
        'INSIDE_BLINK',
      ],
      'sources': [
        '<@(heap_files)',
      ],
    },
  ],
}
