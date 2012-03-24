# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'sync_proto',
      'type': 'static_library',
      'sources': [
        'app_notification_specifics.proto',
        'app_setting_specifics.proto',
        'app_specifics.proto',
        'autofill_specifics.proto',
        'bookmark_specifics.proto',
        'client_commands.proto',
        'client_debug_info.proto',
        'get_updates_caller_info.proto',
        'encryption.proto',
        'extension_setting_specifics.proto',
        'extension_specifics.proto',
        'nigori_specifics.proto',
        'password_specifics.proto',
        'preference_specifics.proto',
        'search_engine_specifics.proto',
        'session_specifics.proto',
        'sync.proto',
        'sync_enums.proto',
        'test.proto',
        'theme_specifics.proto',
        'typed_url_specifics.proto',
      ],
      'variables': {
        'proto_out_dir': 'sync/protocol',
      },
      'includes': ['../../build/protoc.gypi'],
    },
  ],
}
