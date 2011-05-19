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
      'target_name': 'isimpledom',
      'type': 'static_library',
      'msvs_guid': '1814CEEB-49E2-407F-AF99-FA755A7D2607',
      'sources': [
        'ISimpleDOMDocument.idl',
        'ISimpleDOMNode.idl',
        'ISimpleDOMText.idl',
        '<(INTERMEDIATE_DIR)/ISimpleDOMDocument.h',
        '<(INTERMEDIATE_DIR)/ISimpleDOMDocument_i.c',
        '<(INTERMEDIATE_DIR)/ISimpleDOMDocument_p.c',
        '<(INTERMEDIATE_DIR)/ISimpleDOMNode.h',
        '<(INTERMEDIATE_DIR)/ISimpleDOMNode_i.c',
        '<(INTERMEDIATE_DIR)/ISimpleDOMNode_p.c',
        '<(INTERMEDIATE_DIR)/ISimpleDOMText.h',
        '<(INTERMEDIATE_DIR)/ISimpleDOMText_i.c',
        '<(INTERMEDIATE_DIR)/ISimpleDOMText_p.c',
      ],
      'hard_dependency': 1,
      'direct_dependent_settings': {
        'include_dirs': [
          # Bit of a hack to work around the built in vstudio rule.
          '<(INTERMEDIATE_DIR)/../isimpledom',
        ],
      },
      'msvs_settings': {
        'VCMIDLTool': {
          'GenerateTypeLibrary': 'false',
        },
      },
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
