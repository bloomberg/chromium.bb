# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'rdfa_validator',
      'type': 'static_library',
      'sources': [
        'validator_features_all.c',
        'validator_features_validator.c',
        'gen/validator_x86_32.c',
        'gen/validator_x86_64.c',
      ],
    },
  ],
}

