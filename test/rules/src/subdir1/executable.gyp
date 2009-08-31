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
      'sources': [
        'program.c',
        'function1.in',
        'function2.in',
      ],
      'rules': [
        {
          'rule_name': 'copy_file',
          'extension': 'in',
          'inputs': [
            '../copy-file.py',
          ],
          'outputs': [
            # TODO:  fix SCons and Make to support generated files not
            # in a variable-named path like <(INTERMEDIATE_DIR)
            #'<(RULE_INPUT_ROOT).c',
            '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).c',
          ],
          'action': [
            'python', '<(_inputs)', '<(RULE_INPUT_PATH)', '<@(_outputs)',
          ],
          'process_outputs_as_sources': 1,
        },
      ],
    },
  ],
}
