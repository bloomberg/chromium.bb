{
  'targets': [
    {
      'target_name': 'subdir_includes',
      'type': 'executable',
      'include_dirs': [
        '.',
        '../inc1',
        'inc2',
      ],
      'sources': [
        'subdir_includes.c',
      ],
    },
  ],
}
