# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'sources/': [
      ['exclude', 'lame'],
    ],
  },
  'targets': [
    {
      'target_name': 'program',
      'type': 'executable',
      'dependencies': [
        'program_inc',
      ],
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'sources': [
        'program.cc',
      ],
    },
    {
      'target_name': 'program_inc',
      'type': 'none',
      'dependencies': ['cross_program'],
      'actions': [
        {
          'action_name': 'program_inc',
          'inputs': ['<(SHARED_INTERMEDIATE_DIR)/cross_program.fake'],
          'outputs': ['<(SHARED_INTERMEDIATE_DIR)/cross_program.h'],
          'action': ['python', 'tochar.py', '<@(_inputs)', '<@(_outputs)'],
        },
      ],
      # Allows the test to run without hermetic cygwin on windows.
      'msvs_cygwin_shell': 0,
    },
    {
      'target_name': 'cross_program',
      'type': 'none',
      'dependencies': ['cross_lib'],
      'sources': [
        'test1.cc',
        'test2.c',
        'very_lame.cc',
        '<(SHARED_INTERMEDIATE_DIR)/cross_lib.fake',
      ],
      'includes': ['cross_compile.gypi'],
    },
    {
      'target_name': 'cross_lib',
      'type': 'none',
      'sources': [
        'test3.cc',
        'test4.c',
        'bogus1.cc',
        'bogus2.c',
        'sort_of_lame.cc',
      ],
      'sources!': [
        'bogus1.cc',
        'bogus2.c',
      ],
      'includes': ['cross_compile.gypi'],
    },
  ],
}
