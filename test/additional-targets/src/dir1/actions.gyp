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
  ],
}
