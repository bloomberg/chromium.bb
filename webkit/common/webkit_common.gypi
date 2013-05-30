# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../common/blob/webkit_common_blob.gypi',
    '../common/database/webkit_common_database.gypi',
    '../common/fileapi/webkit_common_fileapi.gypi',
    '../common/quota/webkit_common_quota.gypi',
  ],
  # TODO(michaeln): Have webkit_browser target and deprecate old gypis like
  # webkit_storage.gypi.
  'variables': {
    'webkit_common_storage_sources': [
      '<@(webkit_common_blob_sources)',
      '<@(webkit_common_database_sources)',
      '<@(webkit_common_fileapi_sources)',
      '<@(webkit_common_quota_sources)',
    ],
  },
}
