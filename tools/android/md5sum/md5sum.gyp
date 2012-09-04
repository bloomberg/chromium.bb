# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'md5sum',
      'type': 'none',
      'dependencies': [
        'md5sum_stripped_device_bin',
        'md5sum_bin_host#host',
      ],
    },
    {
      'target_name': 'md5sum_device_bin',
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',
      ],
      'include_dirs': [
        '../../..',
      ],
      'sources': [
        'md5sum.cc',
      ],
    },
    {
      'target_name': 'md5sum_stripped_device_bin',
      'type': 'none',
      'dependencies': [
        'md5sum_device_bin',
      ],
      'actions': [
        {
          'action_name': 'strip_md5sum_device_bin',
          'inputs': ['<(PRODUCT_DIR)/md5sum_device_bin'],
          'outputs': ['<(PRODUCT_DIR)/md5sum_bin'],
          'action': [
            '<(android_strip)',
            '--strip-unneeded',
            '<@(_inputs)',
            '-o',
            '<@(_outputs)',
          ],
        },
      ],
    },
    # Same binary but for the host rather than the device.
    {
      'target_name': 'md5sum_bin_host',
      'toolsets': ['host'],
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',
      ],
      'include_dirs': [
        '../../..',
      ],
      'sources': [
        'md5sum.cc',
      ],
    },
  ],
}
