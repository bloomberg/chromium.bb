{
  'targets': [
    {
      'target_name': 'action1_target',
      'type': 'none',
      'suppress_wildcard': 1,
      'actions': [
        {
          'action_name': 'action1',
          'inputs': [
            'emit.py',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/out.txt',
          ],
          'action': ['python', 'emit.py', '<(PRODUCT_DIR)/out.txt'],
          'msvs_cygwin_shell': 0,
        },
      ],
    },
    {
      'target_name': 'action2_target',
      'type': 'none',
      'actions': [
        {
          'action_name': 'action2',
          'inputs': [
            'emit.py',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/out2.txt',
          ],
          'action': ['python', 'emit.py', '<(PRODUCT_DIR)/out2.txt'],
          'msvs_cygwin_shell': 0,
        },
      ],
    },
    {
      'target_name': 'lib1',
      'type': 'shared_library',
      'suppress_wildcard': 1,
      'sources': ['lib1.c'],
    },
  ],
  'conditions': [
    ['OS=="linux"', {
      'target_defaults': {
        'cflags': ['-m32'],
        'ldflags': ['-m32'],
      },
    }],
  ],
}
