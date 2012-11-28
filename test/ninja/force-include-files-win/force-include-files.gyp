# Copyright (c) 2010 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'my_target',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'ForcedIncludeFiles': ['string', 'vector', 'list'],
        },
      },
      'sources': [
        'hello.cc',
      ],
    },
  ],
}
