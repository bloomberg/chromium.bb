# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../browser/appcache/webkit_browser_appcache.gypi',
    '../browser/blob/webkit_browser_blob.gypi',
    '../browser/database/webkit_browser_database.gypi',
    '../browser/dom_storage/webkit_browser_dom_storage.gypi',
    '../browser/fileapi/webkit_browser_fileapi.gypi',
    '../browser/quota/webkit_browser_quota.gypi',
  ],
  # TODO(kinuko): Have webkit_browser target and deprecate old gypis like
  # webkit_storage.gypi.
  'variables': {
    'webkit_browser_storage_sources': [
      '<@(webkit_browser_appcache_sources)',
      '<@(webkit_browser_blob_sources)',
      '<@(webkit_browser_database_sources)',
      '<@(webkit_browser_dom_storage_sources)',
      '<@(webkit_browser_fileapi_sources)',
      '<@(webkit_browser_quota_sources)',
    ],
  },
}
