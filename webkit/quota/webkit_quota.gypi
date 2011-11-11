# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'quota',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/sql/sql.gyp:sql',
      ],
      'sources': [
        'quota_client.h',
        'quota_database.cc',
        'quota_database.h',
        'quota_manager.cc',
        'quota_manager.h',
        'quota_task.cc',
        'quota_task.h',
        'quota_temporary_storage_evictor.cc',
        'quota_temporary_storage_evictor.h',
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
