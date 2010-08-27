# Copyright 2010, Google Inc.
# Copyright 2009 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

{
  # ----------------------------------------------------------------------
  # Default settings
  # ----------------------------------------------------------------------

  'includes': [
    '../../../build/common.gypi',
    ],


  # ----------------------------------------------------------------------
  # actual targets
  # ----------------------------------------------------------------------
  'targets': [
    {
      'target_name': 'nacl_perf_counter',
      'type': 'static_library',
      'sources': [
        'nacl_perf_counter.c',
      ],
    },
  ],
}
