{
  'target_defaults': {
    # TODO:  Necessary definitions for <(INTERMEDIATE_DIR) to work
    # and to have separate func.obj files for the two targets below.
    # These should probably be part of GYP itself, or perhaps
    # moved into the TestGyp.py infrastructure.
    'msvs_configuration_attributes': {
      'OutputDirectory': '$(SolutionDir)$(ConfigurationName)',
      'IntermediateDirectory': '$(OutDir)\\obj\\$(ProjectName)',
    },
  },
  'targets': [
    {
      'target_name': 'prog1',
      'type': 'executable',
      'defines': [
        'PROG="prog1"',
      ],
      'sources': [
        'prog1.c',
        'func.c',
        # Uncomment to test same-named files in different directories,
        # which Visual Studio doesn't support.
        #'subdir1/func.c',
        #'subdir2/func.c',
      ],
    },
    {
      'target_name': 'prog2',
      'type': 'executable',
      'defines': [
        'PROG="prog2"',
      ],
      'sources': [
        'prog2.c',
        'func.c',
        # Uncomment to test same-named files in different directories,
        # which Visual Studio doesn't support.
        #'subdir1/func.c',
        #'subdir2/func.c',
      ],
    },
  ],
}
