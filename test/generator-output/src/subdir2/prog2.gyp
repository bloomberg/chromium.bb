{
  'includes': [
    '../symroot.gypi',
  ],
  'targets': [
    {
      'target_name': 'prog2',
      'type': 'executable',
      'dependencies': [
        '../subdir3/prog3.gyp:prog3',
      ],
      'sources': [
        'prog2.c',
      ],
    },
  ],
}
