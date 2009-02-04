{
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'modp_b64',
      'type': 'static_library',
      'sources': [
        'modp_b64.cc',
        'modp_b64.h',
        'modp_b64_data.h',
      ],
      'vs_props': [ '../../build/external_code.vsprops' ],
    },
  ],
}
