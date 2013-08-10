# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    # TODO(stuartmorgan): All dependencies from code built on iOS to
    # webkit/ should be removed, at which point this condition can be
    # removed.
    ['OS != "ios"', {
      'includes': [
        '../../build/win_precompile.gypi',
        '../glue/webkit_glue.gypi',
        'webkit_support.gypi',
      ],
    }],
  ],
}
