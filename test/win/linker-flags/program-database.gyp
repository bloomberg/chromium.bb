# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    # Verify that 'ProgramDataBase' option correctly makes it to LINK steup in Ninja
    {
      'target_name': 'test_pdb_set',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3'
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'ProgramDatabaseFile': '<(PRODUCT_DIR)\\name_set.pdb',
        },
      },
    },
  ]
}
