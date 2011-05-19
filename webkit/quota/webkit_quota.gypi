# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'quota',
      'type': 'static_library',
      'msvs_guid': '36A9AAD2-10DD-42CF-BF51-3CC79D26FCB4',
      'dependencies': [
        '<(DEPTH)/app/app.gyp:app_base',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/net/net.gyp:net',
      ],
      'sources': [
        'quota_client.h',
        'quota_database.cc',
        'quota_database.h',
        'quota_manager.cc',
        'quota_manager.h',
        'quota_task.cc',
        'quota_task.h',
        'quota_types.h',
        'special_storage_policy.cc',
        'special_storage_policy.h',
        'usage_tracker.cc',
        'usage_tracker.h',
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
