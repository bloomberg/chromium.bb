{
  'includes': [
    'symroot.gypi',
  ],
  'targets': [
    {
      'target_name': 'prog1',
      'type': 'executable',
      'dependencies': [
        'subdir2/prog2.gyp:prog2',
      ],
      'include_dirs': [
        '.',
        'inc1',
        'subdir2/inc2',
        'subdir3/inc3',
        'subdir2/deeper',
      ],
      'sources': [
        'prog1.c',
      ],
    },
  ],
}
