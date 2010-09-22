# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'defines': [
      'HAVE_STDLIB_H',
      'HAVE_STRING_H',
    ],
    'include_dirs': [
      './config',
      './src/include',
      './src/crypto/include',
    ],
    'conditions': [
      ['target_arch=="x64"', {
        'defines': [
          'CPU_CISC',
          'SIZEOF_UNSIGNED_LONG=8',
          'SIZEOF_UNSIGNED_LONG_LONG=8',
        ],
      }],
      ['target_arch=="ia32"', {
        'defines': [
          'CPU_CISC',
          'SIZEOF_UNSIGNED_LONG=4',
          'SIZEOF_UNSIGNED_LONG_LONG=8',
        ],
      }],
      ['target_arch=="arm"', {
        'defines': [
          'CPU_RISC',
          'SIZEOF_UNSIGNED_LONG=4',
          'SIZEOF_UNSIGNED_LONG_LONG=8',
        ],
      }],
      ['OS!="win" and target_arch=="ia32"', {
        'defines': [
          'HAVE_X86',
         ],
      }],
      ['OS!="win"', {
        'defines': [
          'HAVE_STDINT_H',
          'HAVE_INTTYPES_H',
         ],
      }],
      ['OS=="win"', {
        'defines': [
          'HAVE_WINSOCK2_H',
          'inline=__inline',
         ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'libsrtp',
      'type': '<(library)',
      'sources': [
        'src/srtp/ekt.c',
        'src/srtp/srtp.c',
        'src/tables/aes_tables.c',
        'src/crypto/cipher/aes_icm.c',
        'src/crypto/cipher/null_cipher.c',
        'src/crypto/cipher/aes_cbc.c',
        'src/crypto/cipher/cipher.c',
        'src/crypto/cipher/aes.c',
        'src/crypto/math/datatypes.c',
        'src/crypto/math/gf2_8.c',
        'src/crypto/math/stat.c',
        'src/crypto/replay/rdbx.c',
        'src/crypto/replay/ut_sim.c',
        'src/crypto/replay/rdb.c',
        'src/crypto/kernel/alloc.c',
        'src/crypto/kernel/key.c',
        'src/crypto/kernel/err.c',
        'src/crypto/kernel/crypto_kernel.c',
        'src/crypto/rng/rand_source.c',
        'src/crypto/rng/prng.c',
        'src/crypto/rng/ctr_prng.c',
        'src/crypto/ae_xfm/xfm.c',
        'src/crypto/hash/sha1.c',
        'src/crypto/hash/hmac.c',
        'src/crypto/hash/null_auth.c',
        'src/crypto/hash/auth.c',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
