{
  'targets': [
    {
      'target_name': 'program',
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
          'action_name': 'make-program',
          'inputs': [
            'make-program.py',
          ],
          'outputs': [
            # TODO:  fix SCons and Make to support generated files not
            # in a variable-named path like <(INTERMEDIATE_DIR)
            #'program.c',
            '<(INTERMEDIATE_DIR)/program.c',
          ],
          'action': [
            'python', '<(_inputs)', '<@(_outputs)',
          ],
          'process_outputs_as_sources': 1,
        },
      ],
    },
  ],
}
