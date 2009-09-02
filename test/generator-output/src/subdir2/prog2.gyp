{
  'includes': [
    '../symroot.gypi',
  ],
  'targets': [
    {
      'target_name': 'prog2',
      'type': 'executable',
      'include_dirs': [
        '..',
        '../inc1',
        'inc2',
        '../subdir3/inc3',
        'deeper',
      ],
      'dependencies': [
        '../subdir3/prog3.gyp:prog3',
      ],
      'sources': [
        'prog2.c',
      ],
    },
  ],
}
