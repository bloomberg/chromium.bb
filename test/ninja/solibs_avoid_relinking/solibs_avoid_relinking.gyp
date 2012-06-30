# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'a',
      'type': 'shared_library',
      'sources': [ 'solib.cc' ],
    },
    {
      'target_name': 'b',
      'type': 'executable',
      'sources': [ 'main.cc' ],
      'dependencies': [ 'a' ]
    },
  ],
  'conditions': [
    ['OS=="linux"', {
      'target_defaults': {
        'cflags': ['-fPIC'],
      },
    }],
  ],
}
