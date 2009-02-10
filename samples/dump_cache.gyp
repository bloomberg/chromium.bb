{
  'variables': {
    'depth': '../../..',
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'dump_cache',
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../net/net.gyp:net',
      ],
      'sources': [
        'dump_cache.cc',
        'dump_files.cc',
        'upgrade.cc',
      ],
    },
  ],
}
