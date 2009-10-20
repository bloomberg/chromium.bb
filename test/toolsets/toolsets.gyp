{
  'target_defaults': {
    'target_conditions': [
      ['_toolset=="target"', {'defines': ['TARGET']}]
    ]
  },
  'targets': [
    {
      'target_name': 'toolsets',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'toolsets.cc',
      ],
    },
    {
      'target_name': 'host-main',
      'type': 'executable',
      'toolsets': ['host'],
      'dependencies': ['toolsets'],
      'sources': [
        'main.cc',
      ],
    },
    {
      'target_name': 'target-main',
      'type': 'executable',
      'dependencies': ['toolsets'],
      'sources': [
        'main.cc',
      ],
    },
  ],
}
