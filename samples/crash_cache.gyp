{
  'variables': {
    'depth': '../../..',
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'crash_cache',
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../net/net.gyp:net',
      ],
      'sources': [
        'crash_cache.cc',
        '../../disk_cache/disk_cache_test_util.cc',
      ],
    },
  ],
}
