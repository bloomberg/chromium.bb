{
  'targets': [
    {
      'target_name': 'null_input',
      'type': 'executable',
      'msvs_cygwin_shell': 0,
      'actions': [
        {
          'action_name': 'generate_main',
          'process_outputs_as_sources': 1,
          'inputs': [],
          'outputs': [
            '<(INTERMEDIATE_DIR)/main.c',
          ],
          'action': [
            # TODO:  we can't just use <(_outputs) here?!
            'python', 'generate_main.py', '<(INTERMEDIATE_DIR)/main.c',
          ],
        },
      ],
    },
  ],
}
