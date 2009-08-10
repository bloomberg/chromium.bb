{
  'targets': [
    {
      'target_name': 'tools',
      'type': 'executable',
      'sources': [
        'tools.c',
      ],
    },
  ],
  'scons_settings': {
    'tools': ['default', 'this_tool'],
  },
}
