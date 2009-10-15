{
  'targets': [
    {
      'target_name': 'null_input',
      'type': 'executable',
      'msvs_cygwin_shell': 0,
      # TODO:  Necessary definitions for <(INTERMEDIATE_DIR) to work.
      # These should probably be part of GYP itself, or perhaps
      # moved into the TestGyp.py infrastructure.
      'msvs_configuration_attributes': {
        'OutputDirectory': '$(SolutionDir)$(ConfigurationName)',
        'IntermediateDirectory': '$(OutDir)\\obj\\$(ProjectName)',
      },
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
