# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  # This file is the same as webkit_support.gyp except it references
  # features.gypi based on its location in a chromium checkout.  If you
  # add .gypi files here, please add them in webkit_support.gyp as well.
  'includes': [
    '../../third_party/WebKit/WebKit/chromium/features.gypi',
    '../appcache/webkit_appcache.gypi',
    '../database/webkit_database.gypi',
    '../glue/webkit_glue.gypi',
    'webkit_support.gypi',
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
