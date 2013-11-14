# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'purge_ashmem',
      'type': 'executable',
      'dependencies': [
        '../../../third_party/ashmem/ashmem.gyp:ashmem',
      ],
      'include_dirs': [
        '../../../',
      ],
      'conditions': [
        # Warning: A PIE tool cannot run on ICS 4.0.4, so only
        #          build it as position-independent when ASAN
        #          is activated. See b/6587214 for details.
        [ 'asan==1', {
          'cflags': [
            '-fPIE',
          ],
          'ldflags': [
            '-pie',
          ],
        }],
      ],
      'sources': [
        'purge_ashmem.c',
      ],
    },
  ],
}
