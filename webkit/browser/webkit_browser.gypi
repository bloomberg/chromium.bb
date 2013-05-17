# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../browser/fileapi/webkit_browser_fileapi.gypi',
  ],
  # TODO(kinuko): Have webkit_browser target and deprecate old gypis like
  # webkit_storage.gypi.
  'variables': {
    'webkit_browser_storage_sources': [
      '<@(webkit_browser_fileapi_sources)',
    ],
  },
}
