# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_output_exe',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(OutDir)\\blorp.exe'
        },
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_output_dll',
      'type': 'shared_library',
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(OutDir)\\blorp.dll'
        },
      },
      'sources': ['hello.cc'],
    },
  ]
}
