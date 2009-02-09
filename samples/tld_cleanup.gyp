{
  'variables': {
    'depth': '../../..',
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'tld_cleanup',
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../googleurl/build/googleurl.gyp:googleurl',
      ],
      'sources': [
        'tld_cleanup.cc',
      ],
    },
  ],
}
