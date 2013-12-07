# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target for instrumented dynamic
# libraries and describes standard build action for most of the libraries.

{
  'target_name': '<(_sanitizer_type)-<(_library_name)',
  'type': 'none',
  'actions': [
    {
      'action_name': '<(_library_name)',
      'inputs': [
        'download_build_install.py',
      ],
      'outputs': [
        '<(PRODUCT_DIR)/instrumented_libraries/<(_sanitizer_type)/<(_library_name).txt',
      ],
      'action': ['<(DEPTH)/third_party/instrumented_libraries/download_build_install.py',
        '--product-directory=<(PRODUCT_DIR)',
        '--library=<(_library_name)',
        '--intermediate-directory=<(INTERMEDIATE_DIR)',
        '--sanitizer-type=<(_sanitizer_type)',
        '<(_verbose_libraries_build_flag)',
      ],
    },
  ],
}
