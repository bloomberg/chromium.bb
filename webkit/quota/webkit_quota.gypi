# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'quota',
      'type': '<(library)',
      'msvs_guid': '36A9AAD2-10DD-42CF-BF51-3CC79D26FCB4',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
      ],
      'sources': [
        'special_storage_policy.cc',
        'special_storage_policy.h',
      ],
      'conditions': [
        ['inside_chromium_build==0', {
          'dependencies': [
            '<(DEPTH)/webkit/support/setup_third_party.gyp:third_party_headers',
          ],
        }],
      ],
    },
  ],
}
