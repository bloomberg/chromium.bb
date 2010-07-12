# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../../build/common.gypi',
    '../../build/version.gypi',
  ],
  'targets': [
    {
      'target_name': 'installer',
      'type': 'none',
      'dependencies': [
        '../../converter/converter.gyp:o3dConverter',
        '../../breakpad/breakpad.gyp:reporter',
        '../../documentation/documentation.gyp:*',
        '../../plugin/plugin.gyp:npo3dautoplugin',
        '../../samples/samples.gyp:samples',
      ],
      'postbuilds': [
        {
          'variables': {
            'installer_script_path': './make_installer.sh',
          },
          'postbuild_name': 'Make Installer',
          'action': ['<(installer_script_path)', '<(plugin_version)',],
        },
      ],
    },
    {
      'target_name': 'disk_image',
      'type': 'none',
      'dependencies': [
        'installer',
      ],
      'postbuilds': [
        {
          'variables': {
            'disk_image_script_path': './make_disk_image.sh',
          },
          'postbuild_name': 'Make Disk Image',
          'action': ['<(disk_image_script_path)', '<(plugin_version)',],
        },
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
