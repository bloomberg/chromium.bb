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
        # TODO(earthdok): reintroduce some sort of dependency
        # See http://crbug.com/343515
        #'download_build_install.py',
      ],
      'outputs': [
        '<(PRODUCT_DIR)/instrumented_libraries/<(_sanitizer_type)/<(_library_name).txt',
      ],
      'action': ['<(DEPTH)/third_party/instrumented_libraries/download_build_install.py',
        '--product-directory=<(PRODUCT_DIR)',
        '--library=<(_library_name)',
        '--intermediate-directory=<(INTERMEDIATE_DIR)',
        '--sanitizer-type=<(_sanitizer_type)',
        '--custom-configure-flags=<(_custom_configure_flags)',
        '--custom-c-compiler-flags=<(_custom_c_compiler_flags)',
        '--custom-cxx-compiler-flags=<(_custom_cxx_compiler_flags)',
        '--custom-linker-flags=<(_custom_linker_flags)',
        '--run-before-build=<(_run_before_build)',
        '<(_verbose_libraries_build_flag)',
        '--c-compiler=<(_cc)',
        '--cxx-compiler=<(_cxx)',
        '--jobs=<(_jobs)',
        '--build-method=<(_build_method)',
      ],
    },
  ],
}
