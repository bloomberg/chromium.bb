# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'libssl',
      'type': '<(component)',
      'product_name': 'crssl',  # Don't conflict with OpenSSL's libssl
      'sources': [
        'ssl/SSLerrs.h',
        'ssl/authcert.c',
        'ssl/cmpcert.c',
        'ssl/derive.c',
        'ssl/dtlscon.c',
        'ssl/preenc.h',
        'ssl/prelib.c',
        'ssl/ssl.h',
        'ssl/ssl3con.c',
        'ssl/ssl3ecc.c',
        'ssl/ssl3ext.c',
        'ssl/ssl3gthr.c',
        'ssl/ssl3prot.h',
        'ssl/sslauth.c',
        'ssl/sslcon.c',
        'ssl/ssldef.c',
        'ssl/sslenum.c',
        'ssl/sslerr.c',
        'ssl/sslerr.h',
        'ssl/sslerrstrs.c',
        'ssl/sslgathr.c',
        'ssl/sslimpl.h',
        'ssl/sslinfo.c',
        'ssl/sslinit.c',
        'ssl/sslmutex.c',
        'ssl/sslmutex.h',
        'ssl/sslnonce.c',
        'ssl/sslproto.h',
        'ssl/sslreveal.c',
        'ssl/sslsecur.c',
        'ssl/sslsnce.c',
        'ssl/sslsock.c',
        'ssl/sslt.h',
        'ssl/ssltrace.c',
        'ssl/sslver.c',
        'ssl/tls13con.c',
        'ssl/tls13con.h',
        'ssl/tls13hkdf.c',
        'ssl/tls13hkdf.h',
        'ssl/unix_err.c',
        'ssl/unix_err.h',
      ],
      'defines': [
        'NO_PKCS11_BYPASS',
        'NSS_ENABLE_ECC',
        'USE_UTIL_DIRECTLY',
      ],
      'variables': {
        'clang_warning_flags_unset': [
          # ssl uses PR_ASSERT(!"foo") instead of PR_ASSERT(false && "foo")
          '-Wstring-conversion',
        ],
      },
      'conditions': [
        ['component == "shared_library"', {
          'conditions': [
            ['OS == "ios"', {
              'xcode_settings': {
                'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',
              },
            }],
          ],
        }],
        [ 'clang == 1', {
          'cflags': [
            # There is a broken header guard in /usr/include/nss/secmod.h:
            # https://bugzilla.mozilla.org/show_bug.cgi?id=884072
            '-Wno-header-guard',
          ],
        }],
        [ 'OS == "ios"', {
          'defines': [
            'XP_UNIX',
            'DARWIN',
            'XP_MACOSX',
          ],
        }],
        [ 'OS == "ios"', {
          'dependencies': [
            '../../../third_party/nss/nss.gyp:nspr',
            '../../../third_party/nss/nss.gyp:nss',
          ],
          'export_dependent_settings': [
            '../../../third_party/nss/nss.gyp:nspr',
            '../../../third_party/nss/nss.gyp:nss',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'ssl',
            ],
          },
        }],
      ],
      'configurations': {
        'Debug_Base': {
          'defines': [
            'DEBUG',
          ],
        },
      },
    },
  ],
}
