{
  'targets': [
    {
      'target_name': 'program',
      'type': 'executable',
      'dependencies': [
        'library',
      ],
      'sources': [
        'program.c',
      ],
    },
    {
      'target_name': 'library',
      'type': '<(library)',
      'sources': [
        'library.c',
      ],
    },
  ],
}
