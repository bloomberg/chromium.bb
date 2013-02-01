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
  'dependencies': [
    '../base/base.gyp:base',
  ],
  'sources': [
    'api/string_ordinal.h',
    'api/syncable_service.cc',
    'api/syncable_service.h',
    'api/sync_data.h',
    'api/sync_data.cc',
    'api/sync_change.h',
    'api/sync_change.cc',
    'api/sync_change_processor.h',
    'api/sync_change_processor.cc',
    'api/sync_error.h',
    'api/sync_error.cc',
    'api/sync_error_factory.h',
    'api/sync_error_factory.cc',
    'api/sync_merge_result.h',
    'api/sync_merge_result.cc',
    'api/time.h',
  ],
}
