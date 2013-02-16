# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'include_dirs': [
    '..',
  ],
  'defines': [
    'SYNC_IMPLEMENTATION',
  ],
  'sources': [
    'protocol/app_notification_specifics.proto',
    'protocol/app_setting_specifics.proto',
    'protocol/app_specifics.proto',
    'protocol/autofill_specifics.proto',
    'protocol/bookmark_specifics.proto',
    'protocol/client_commands.proto',
    'protocol/client_debug_info.proto',
    'protocol/device_info_specifics.proto',
    'protocol/dictionary_specifics.proto',
    'protocol/encryption.proto',
    'protocol/experiments_specifics.proto',
    'protocol/extension_setting_specifics.proto',
    'protocol/extension_specifics.proto',
    'protocol/favicon_image_specifics.proto',
    'protocol/favicon_tracking_specifics.proto',
    'protocol/get_updates_caller_info.proto',
    'protocol/history_delete_directive_specifics.proto',
    'protocol/nigori_specifics.proto',
    'protocol/password_specifics.proto',
    'protocol/preference_specifics.proto',
    'protocol/priority_preference_specifics.proto',
    'protocol/search_engine_specifics.proto',
    'protocol/session_specifics.proto',
    'protocol/sync.proto',
    'protocol/sync_enums.proto',
    'protocol/synced_notification_data.proto',
    'protocol/synced_notification_render.proto',
    'protocol/synced_notification_specifics.proto',
    'protocol/test.proto',
    'protocol/theme_specifics.proto',
    'protocol/typed_url_specifics.proto',
    'protocol/unique_position.proto',
  ],
  'variables': {
    'proto_in_dir': './protocol',
    'proto_out_dir': 'sync/protocol',
    'cc_generator_options': 'dllexport_decl=SYNC_EXPORT:',
    'cc_include': 'sync/base/sync_export.h',
  },
  'includes': [
    '../build/protoc.gypi'
  ],
}
