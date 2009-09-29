{
  'targets': [
    {
      'target_name': 'program',
      'type': 'executable',
      'dependencies': [
        'lib1'
      ],
      'sources': [
        'program.cpp',
      ],
    },
    {
      'target_name': 'lib1',
      'type': 'static_library',
      'sources': [
        'lib1.hpp',
        'lib1.cpp',
      ],
    },
  ],
}
