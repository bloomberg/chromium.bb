{
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'activex_shim_dll',
      'type': 'shared_library',
      'dependencies': [
        '../../third_party/npapi/npapi.gyp:npapi',
        '../activex_shim/activex_shim.gyp:activex_shim',
      ],
      'sources': [
        'activex_shim_dll.cc',
        'activex_shim_dll.def',
        'activex_shim_dll.rc',
        'resource.h',
      ],
    },
  ],
}
