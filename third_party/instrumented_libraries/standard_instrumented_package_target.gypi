# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target for instrumented dynamic
# packages and describes standard build action for most of the packages.

{
  'target_name': '<(_sanitizer_type)-<(_package_name)',
  'type': 'none',
  'actions': [
    {
      'action_name': '<(_package_name)',
      'inputs': [
        # TODO(earthdok): reintroduce some sort of dependency
        # See http://crbug.com/343515
        #'download_build_install.py',
      ],
      'outputs': [
        '<(PRODUCT_DIR)/instrumented_libraries/<(_sanitizer_type)/<(_package_name).txt',
      ],
      'action': ['<(DEPTH)/third_party/instrumented_libraries/download_build_install.py',
        '--product-directory=<(PRODUCT_DIR)',
        '--package=<(_package_name)',
        '--intermediate-directory=<(INTERMEDIATE_DIR)',
        '--sanitizer-type=<(_sanitizer_type)',
        '--extra-configure-flags=>(_extra_configure_flags)',
        '--cflags=>(_package_cflags)',
        '--ldflags=>(_package_ldflags)',
        '--cc=<(_cc)',
        '--cxx=<(_cxx)',
        '--jobs=>(_jobs)',
        '--build-method=>(_build_method)',
      ],
      'conditions': [
        ['verbose_libraries_build==1', {
          'action+': [
            '--verbose',
          ],
        }],
      ],
      'target_conditions': [
        ['">(_patch)"!=""', {
          'action+': [
            '--patch=>(_patch)',
          ],
          'inputs+': [
            '>(_patch)',
          ],
        }],
        ['">(_run_before_build)"!=""', {
          'action+': [
            '--run-before-build=>(_run_before_build)',
          ],
          'inputs+': [
            '>(_run_before_build)',
          ],
        }],
        ['">(_<(_sanitizer_type)_blacklist)"!=""', {
          'action+': [
            '--sanitizer-blacklist=>(_<(_sanitizer_type)_blacklist)',
          ],
          'inputs+': [
            '>(_<(_sanitizer_type)_blacklist)',
          ],
        }],
      ],
    },
  ],
}
