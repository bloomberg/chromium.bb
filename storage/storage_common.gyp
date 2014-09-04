# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //storage/common
      'target_name': 'storage_common',
      'type': '<(component)',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/url/url.gyp:url_lib',
      ],
      'defines': ['STORAGE_COMMON_IMPLEMENTATION'],
      'sources': [
        'common/data_element.cc',
        'common/data_element.h',
        'common/blob/blob_data.cc',
        'common/blob/blob_data.h',
        'common/blob/scoped_file.cc',
        'common/blob/scoped_file.h',
        'common/blob/shareable_file_reference.cc',
        'common/blob/shareable_file_reference.h',
        'common/database/database_connections.cc',
        'common/database/database_connections.h',
        'common/database/database_identifier.cc',
        'common/database/database_identifier.h',
        'common/fileapi/directory_entry.cc',
        'common/fileapi/directory_entry.h',
        'common/fileapi/file_system_info.cc',
        'common/fileapi/file_system_info.h',
        'common/fileapi/file_system_mount_option.h',
        'common/fileapi/file_system_types.h',
        'common/fileapi/file_system_util.cc',
        'common/fileapi/file_system_util.h',
        'common/quota/quota_status_code.cc',
        'common/quota/quota_status_code.h',
        'common/quota/quota_types.h',
        'common/storage_common_export.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ],
}
