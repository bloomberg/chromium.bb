{
  'targets': [
    {
      'target_name': 'bare',
      'type': 'none',
      'actions': [
        {
          'action_name': 'action1',
          'inputs': [
            'bare.py',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/out.txt',
          ],
          'action': ['python', 'bare.py', '<(PRODUCT_DIR)/out.txt'],
          'msvs_cygwin_shell': 0,
        },
      ],
    },
  ],
}
