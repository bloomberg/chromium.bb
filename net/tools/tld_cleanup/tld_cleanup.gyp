# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'tld_cleanup',
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../build/temp_gyp/googleurl.gyp:googleurl',
      ],
      'sources': [
        'tld_cleanup.cc',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
