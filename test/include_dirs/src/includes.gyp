{
  'targets': [
    {
      'target_name': 'includes',
      'type': 'executable',
      'dependencies': [
        'subdir/subdir_includes.gyp:subdir_includes',
      ],
      'include_dirs': [
        '.',
        'inc1',
        'subdir/inc2',
      ],
      'sources': [
        'includes.c',
      ],
    },
  ],
}
