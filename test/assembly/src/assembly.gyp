{
  'target_defaults': {
    'conditions': [
      ['OS=="win"', {
        'defines': ['PLATFORM_WIN'],
      }],
      ['OS=="mac"', {
        'defines': ['PLATFORM_MAC'],
      }],
      ['OS=="linux"', {
        'defines': ['PLATFORM_LINUX'],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'program',
      'type': 'executable',
      'dependencies': ['lib1'],
      'sources': [
        'program.c',
      ],
    },
    {
      'target_name': 'lib1',
      'type': 'static_library',
      'sources': [
        'lib1.S',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'target_defaults': {
        'rules': [
          {
            'rule_name': 'assembler',
            'msvs_cygwin_shell': 0,
            'extension': 'S',
            'inputs': [
              'as.bat',
            ],
            'outputs': [
              '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).obj',
            ],
            'action':
              ['as.bat', 'lib1.c', '<(_outputs)'],
            'message': 'Building assembly file <(RULE_INPUT_PATH)',
            'process_outputs_as_sources': 1,
          },
        ],
      },
    },],
  ],
}
