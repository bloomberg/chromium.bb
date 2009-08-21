{
  'includes': [
    'symroot.gypi',
  ],
  'targets': [
    {
      'target_name': 'prog1',
      'type': 'executable',
      'dependencies': [
        'subdir/prog2.gyp:prog2',
      ],
      'sources': [
        'prog1.c',
      ],
    },
  ],
}
