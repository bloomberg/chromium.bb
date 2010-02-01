# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'variables': {
    'O3D_PLUGIN_VERSION':
        '<!(python ../../../plugin/version_info.py --version)',
    'INSTALLER_DIR': '<(PRODUCT_DIR)/installer',
    'DEBIAN_DIR': '<(INSTALLER_DIR)/debian',
  },
  'conditions': [
    [ 'target_arch=="x64"',
      {
        'variables': { 'ARCH': 'amd64' }
      },
      {
        'variables': { 'ARCH': 'i386' }
      },
    ],
  ],
  'targets': [
    {
      'target_name': 'debian',
      'dependencies': [
        '../../../build/libs.gyp:cg_libs',
        '../../../plugin/plugin.gyp:npo3dautoplugin',
      ],
      'type': 'none',
      'copies': [
        {
          'destination': '<(DEBIAN_DIR)',
          'files': [
            'compat',
            'control',
            'copyright',
            'google-o3d.install',
            'google-o3d.links',
            'google-o3d.lintian-overrides',
            'rules',
          ],
        },
      ],
      'actions': [
        {
          'action_name': 'deb_mk_changelog',
          'inputs': [
            'changelog.in',
            'mk_changelog.py',
          ],
          'outputs': [
            '<(DEBIAN_DIR)/changelog',
          ],
          'action': [
            'python',
            'mk_changelog.py',
            '--version=<(O3D_PLUGIN_VERSION)',
            '--out=<(DEBIAN_DIR)/changelog',
            '--in=changelog.in',
          ],
        },
        {
          'action_name': 'make_deb',
          'inputs': [
            '<(PRODUCT_DIR)/libnpo3dautoplugin.so',
            '<(PRODUCT_DIR)/libCg.so',
            '<(PRODUCT_DIR)/libCgGL.so',
            '<(DEBIAN_DIR)/changelog',
            '<(DEBIAN_DIR)/compat',
            '<(DEBIAN_DIR)/control',
            '<(DEBIAN_DIR)/copyright',
            '<(DEBIAN_DIR)/google-o3d.install',
            '<(DEBIAN_DIR)/google-o3d.links',
            '<(DEBIAN_DIR)/google-o3d.lintian-overrides',
            '<(DEBIAN_DIR)/rules',
          ],
          'action': [
            'cd',
            '<(INSTALLER_DIR)',
            '&&',
            'dpkg-buildpackage',
            '-uc', # Don't sign the changes file
            '-tc', # Clean the tree
            '-b', # Don't produce a source build
            '-a<(ARCH)',
            '-D', # -a suppresses build-dep checking, so turn it back on
            '-rfakeroot',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/google-o3d_<(O3D_PLUGIN_VERSION)_<(ARCH).changes',
            '<(PRODUCT_DIR)/google-o3d_<(O3D_PLUGIN_VERSION)_<(ARCH).deb',
            '<(PRODUCT_DIR)/' +
                'google-o3d-dbgsym_<(O3D_PLUGIN_VERSION)_<(ARCH).deb',
          ],
        },
      ],
    },
  ],
}
