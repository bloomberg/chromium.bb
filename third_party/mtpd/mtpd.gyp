# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'mtpd',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../build/linux/system.gyp:glib',
        '../../build/linux/system.gyp:udev',
        '../../device/device.gyp:mtp_file_entry_proto',
        '../../device/device.gyp:mtp_storage_info_proto',
        '../../third_party/cros_dbus_cplusplus/cros_dbus_cplusplus.gyp:dbus_cplusplus',
        '../../third_party/libmtp/libmtp.gyp:libmtp',
      ],
      'sources': [
        'source/assert_matching_file_types.cc',
        'source/build_config.h',
        'source/daemon.cc',
        'source/daemon.h',
        'source/device_event_delegate.h',
        'source/device_manager.cc',
        'source/device_manager.h',
        'source/file_entry.cc',
        'source/file_entry.h',
        'source/main.cc',
        'source/mtpd_server_impl.cc',
        'source/mtpd_server_impl.h',
        'source/service_constants.h',
        'source/storage_info.cc',
        'source/storage_info.h',
        'source/string_helpers.cc',
        'source/string_helpers.h',
      ],
      'include_dirs': [
        '.',
        '<(SHARED_INTERMEDIATE_DIR)/protoc_out/device/media_transfer_protocol',
      ],
    },
  ],
}
