{
  'targets': [
    {
      'target_name': 'a',
      'type': 'static_library',
      'sources': [
        'a.c',
      ],
      'dependencies': ['b/b.gyp:b'],
    },
  ],
}
