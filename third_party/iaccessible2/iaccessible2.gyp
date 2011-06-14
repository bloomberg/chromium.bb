# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
  ],

  'target_defaults': {
    'include_dirs': [
      '.',
      '<(INTERMEDIATE_DIR)',
    ],
  },
  'targets': [
    {
      'target_name': 'iaccessible2',
      'type': 'static_library',
      'sources': [
        'ia2_api_all.idl',
        '<(INTERMEDIATE_DIR)/ia2_api_all.h',
        '<(INTERMEDIATE_DIR)/ia2_api_all_i.c',
        '<(INTERMEDIATE_DIR)/ia2_api_all_p.c',
      ],
      'hard_dependency': 1,
      'direct_dependent_settings': {
        'include_dirs': [
          # Bit of a hack to work around the built in vstudio rule.
          '<(INTERMEDIATE_DIR)/../iaccessible2',
        ],
      },
    },
    {
      'target_name': 'IAccessible2Proxy',
      'type': 'shared_library',
      'defines': [ 'REGISTER_PROXY_DLL' ],
      'dependencies': [ 'iaccessible2' ],
      'sources': [
        'IAccessible2Proxy.def',
        '<(INTERMEDIATE_DIR)/../iaccessible2/dlldata.c',
      ],
      'link_settings': {
        'libraries': [
          '-lrpcrt4.lib',
        ],
      },
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
