# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'nppversion': '<!(python ../../plugin/version_info.py --commaversion)',
    'dotnppversion': '<!(python ../../plugin/version_info.py --version)',

    # We don't actually want the extras version to update by itself;
    # it should change only when we actually add something to the
    # installer or change the d3dx9 version.  This version is
    # therefore independent of the o3d plugin and sdk versions.
    'extrasversion': '0,1,1,0',
    'dotextrasversion': '0.1.1.0',
  },
  'includes': [
    '../../build/common.gypi',
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
          'action': ['<(installer_script_path)', '<(dotnppversion)',],
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
          'action': ['<(disk_image_script_path)', '<(dotnppversion)',],
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
