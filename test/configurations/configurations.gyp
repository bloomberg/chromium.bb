{
  'targets': [
    {
      'target_name': 'configurations',
      'type': 'executable',
      'sources': [
        'configurations.c',
      ],
      'configurations': {
        'Debug': {
          'defines': [
            'DEBUG',
          ],
        },
        'Release': {
          'defines': [
            'RELEASE',
          ],
        },
        'Foo': {
          'defines': [
            'FOO',
          ],
        },
      }
    },
  ],
}
