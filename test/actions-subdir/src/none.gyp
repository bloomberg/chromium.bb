{
  'targets': [
    {
      'target_name': 'file',
      'type': 'none',
      'msvs_cygwin_shell': 0,
      'actions': [
        {
          'action_name': 'make-file',
          'inputs': [
            'make-file.py',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/file.out',
          ],
          'action': [
            'python', '<(_inputs)', '<@(_outputs)',
          ],
          'process_outputs_as_sources': 1,
        }
      ],
      'dependencies': [
        'subdir/subdir.gyp:subdir_file',
      ],
    },
  ],
}
