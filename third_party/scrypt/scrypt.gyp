# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'libscrypt',
      'type': 'static_library',
      'sources': [
        'lib/crypto/sha256.c',
        'lib/crypto/crypto_scrypt-nosse.c',
      ],
      'include_dirs': [
        '.',
      ],
    },
  ],
}
