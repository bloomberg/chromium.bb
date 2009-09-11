{
  'includes': [
    '../builddir.gypi',
  ],
  'targets': [
    {
      'target_name': 'prog2',
      'type': 'executable',
      'sources': [
        'prog2.c',
        '../func2.c',
      ],
    },
  ],
}
