# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build the specific library dependencies for validating on x86-32 using the
# DFA-based validator.
{
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'defines': [ 'NACL_TRUSTED_BUT_NOT_TCB' ],
  },
  'conditions': [
    ['target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'dfa_validate_x86_32',
          'type': 'static_library',
          'sources' : [
            'unreviewed/dfa_validate_32.c',
            'gen/validator-x86_32.c',
          ],
        },
      ],
    }],
    [ 'target_arch=="arm" or target_arch=="x64"', {
      'targets': []
    }],
  ],
}

