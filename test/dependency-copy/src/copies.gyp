{
  'targets': [
    {
      'target_name': 'proj1',
      'type': 'executable',
      'sources': [
        'file1.c',
      ],
    },
    {
      'target_name': 'proj2',
      'type': 'executable',
      'sources': [
        'file2.c',
      ],
      'dependencies': [
        'proj1',
      ]
    },
  ],
}
