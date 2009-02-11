{
  'variables': {
    'depth': '..',
  },
  'includes': [
    '../build/common.gypi',
  ],
  'target_defaults': {
    'include_dirs': [
      '..',
    ],
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'webkit_resources',
          'type': 'none',
          'sources': [
            'glue/webkit_resources.grd',
          ],
          'msvs_tool_files': ['../tools/grit/build/grit_resources.rules'],
          'direct_dependent_settings': {
            'include_dirs': [
              '$(OutDir)/grit_derived_sources',
            ],
          },
        },
      ],
    }],
  ],
}
