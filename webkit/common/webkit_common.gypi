# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../common/database/webkit_common_database.gypi',
  ],
  # TODO(michaeln): Have webkit_browser target and deprecate old gypis like
  # webkit_storage.gypi.
  'variables': {
    'webkit_common_storage_sources': [
      '<@(webkit_common_database_sources)',
    ],
  },
}
