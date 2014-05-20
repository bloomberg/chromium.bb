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
    '../build/scripts/scripts.gypi',
    '../build/win/precompile.gypi',
    '../bindings/bindings.gypi',
    'modules.gypi',
  ],
  'targets': [{
    'target_name': 'modules',
    'type': 'static_library',
    'dependencies': [
      '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
      '<(DEPTH)/third_party/sqlite/sqlite.gyp:sqlite',
      '../config.gyp:config',
      '../core/core.gyp:webcore',
      'make_modules_generated',
    ],
    'defines': [
      'BLINK_IMPLEMENTATION=1',
      'INSIDE_BLINK',
    ],
    'sources': [
      '<@(modules_files)',
      '<@(bindings_modules_generated_aggregate_files)',
    ],
    # Disable c4267 warnings until we fix size_t to int truncations.
    'msvs_disabled_warnings': [ 4267, 4334, ]
  },
  {
    'target_name': 'modules_testing',
    'type': 'static_library',
    'dependencies': [
      '../config.gyp:config',
      '../core/core.gyp:webcore',
    ],
    'defines': [
      'BLINK_IMPLEMENTATION=1',
      'INSIDE_BLINK',
    ],
    'sources': [
      '<@(modules_testing_files)',
    ],

  },
  {
    'target_name': 'make_modules_generated',
    'type': 'none',
    'hard_dependency': 1,
    'dependencies': [
      #'generated_testing_idls',
      '../bindings/core_bindings_generated.gyp:core_bindings_generated',
      '../config.gyp:config',
    ],
    'sources': [
      # bison rule
      '../core/css/CSSGrammar.y',
      '../core/xml/XPathGrammar.y',
    ],
    'actions': [
      {
        'action_name': 'EventTargetModulesFactory',
        'inputs': [
          '<@(make_event_factory_files)',
          'EventTargetModulesFactory.in',
        ],
        'outputs': [
          '<(SHARED_INTERMEDIATE_DIR)/blink/EventTargetModulesHeaders.h',
          '<(SHARED_INTERMEDIATE_DIR)/blink/EventTargetModulesInterfaces.h',
        ],
        'action': [
          'python',
          '../build/scripts/make_event_factory.py',
          'EventTargetModulesFactory.in',
          '--output_dir',
          '<(SHARED_INTERMEDIATE_DIR)/blink',
        ],
      },
      {
        'action_name': 'EventTargetModulesNames',
        'inputs': [
          '<@(make_names_files)',
          'EventTargetModulesFactory.in',
        ],
        'outputs': [
          '<(SHARED_INTERMEDIATE_DIR)/blink/EventTargetModulesNames.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/blink/EventTargetModulesNames.h',
        ],
        'action': [
          'python',
          '../build/scripts/make_names.py',
          'EventTargetModulesFactory.in',
          '--output_dir',
          '<(SHARED_INTERMEDIATE_DIR)/blink',
        ],
      },
    ],
  }],
}
