# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    # Tests whether midl receives the list of preprocessor defines.
    {
      'target_name': 'test_defines',
      'type': 'static_library',
      'defines': [
        'GENERATED_UUID=ea6cba74-40a8-4c13-abeb-3c121884bbfd'
      ],
      'sources': [
        'defines.idl',
      ],
    },
  ],
}
