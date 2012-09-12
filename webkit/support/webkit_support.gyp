# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../user_agent/webkit_user_agent.gypi',
  ],
  'conditions': [
    # TODO(stuartmorgan): All dependencies from code built on iOS to
    # webkit/ should be removed, at which point this condition can be
    # removed.
    ['OS != "ios"', {
      'includes': [
        '../../build/win_precompile.gypi',
        '../appcache/webkit_appcache.gypi',
        '../base/webkit_base.gypi',
        '../blob/webkit_blob.gypi',
        '../database/webkit_database.gypi',
        '../dom_storage/webkit_dom_storage.gypi',
        '../fileapi/webkit_fileapi.gypi',
        '../forms/webkit_forms.gypi',
        '../glue/webkit_glue.gypi',
        '../gpu/webkit_gpu.gypi',
        '../media/webkit_media.gypi',
        '../quota/webkit_quota.gypi',
        'webkit_support.gypi',
      ],
    }],
  ],
}
