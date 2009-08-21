{
  'targets': [
    {
      # TODO(sgk):  a target name of 'all' leads to a scons dependency cycle
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        '../prog1/prog1.gyp:*',
        '../prog2/prog2.gyp:*',
      ],
    },
  ],
}
