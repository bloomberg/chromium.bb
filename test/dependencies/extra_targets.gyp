{
  'targets': [
    {
      'target_name': 'a',
      'type': 'static_library',
      'sources': [
        'a.c',
      ],
      # This only depends on the "d" target; other targets in c.gyp
      # should not become part of the build (unlike with 'c/c.gyp:*').
      'dependencies': ['c/c.gyp:d'],
    },
  ],
}
