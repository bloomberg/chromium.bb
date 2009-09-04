{
  'includes': [
    '../symroot.gypi',
  ],
  'targets': [
    {
      'target_name': 'prog3',
      'type': 'executable',
      'include_dirs': [
        '..',
        '../inc1',
        '../subdir2/inc2',
        'inc3',
        '../subdir2/deeper',
      ],
      'sources': [
        'prog3.c',
      ],
    },
  ],
}
