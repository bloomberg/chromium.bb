# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    # Tests whether midl receives the list of include directories.
    {
      'target_name': 'test_includes',
      'type': 'static_library',
      'include_dirs': [
        'include'
      ],
      'sources': [
        'additional-include-dirs.idl',
      ],
    },
  ],
}
