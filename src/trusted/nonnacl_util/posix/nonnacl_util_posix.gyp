# Copyright (c) 2009 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'nonnacl_util_posix',
      'type': 'static_library',
      'sources': [
        'get_plugin_dirname.cc',
        'sel_ldr_launcher_posix.cc',
      ],
    },
  ],
}
