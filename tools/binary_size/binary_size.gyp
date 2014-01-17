# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'binary_size_java',
      'type': 'none',
      'variables': {
        'java_in_dir': '../binary_size/java',
      },
      'includes': [ '../../build/java.gypi' ],
    },
  ],
}
