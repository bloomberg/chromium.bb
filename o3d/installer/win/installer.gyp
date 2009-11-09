# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'wixdir': 'third_party\\wix_2_0_4221\\files',
    'dx_redist_path': '../../../o3d-internal/third_party/dx_nov_2007_redist',
    'dx_redist_exists': '<!(python ../../build/file_exists.py ../../../o3d-internal/third_party/dx_nov_2007_redist/d3dx9_36.dll)',
    'guidgen': '..\\..\\nbguidgen\\win\\nbguidgen.exe',
    'nppversion': '<!(python ../../plugin/version_info.py --commaversion)',
    'dotnppversion': '<!(python ../../plugin/version_info.py --version)',

    # Unique guid for o3d namespace
    'o3d_namespace_guid': 'B445DBAE-F5F9-435A-9A9B-088261CDF00A',

    # Changing the following values would break upgrade paths, so we
    # hard-code the values instead of generating them.
    'bad_old_o3d_upgrade_code': 'dc819ed6-4155-3cff-b580-45626aed5848',
    'o3d_extras_google_update_guid': '{34B2805D-C72C-4f81-AED5-5A22D1E092F1}',
    'o3d_extras_upgrade_code': 'c271f2f0-c7ad-3bc9-8216-211436aa2244',
    'o3d_npp_google_update_guid': '{70308795-045C-42da-8F4E-D452381A7459}',
    'o3d_npp_upgrade_code': '0f098121-2876-3c23-bd4c-501220ecbb42',

    # We don't actually want the extras version to update by itself;
    # it should change only when we actually add something to the
    # installer or change the d3dx9 version.  This version is
    # therefore independent of the o3d plugin and sdk versions.
    'extrasversion': '0,1,1,0',
    'dotextrasversion': '0.1.1.0',

    # Registry paths for Google Update
    'google_update_reg_path': 'Software\\Google\\Update\\Clients\\',
    'google_update_state_reg_path': 'Software\\Google\\Update\\ClientState\\',
  },
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'cactions',
      'type': 'shared_library',
      'sources': [
        'custom_actions.cc',
      ],
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../build/libs.gyp:cg_libs',
        '../../build/libs.gyp:gl_libs',
        '../../plugin/plugin.gyp:o3dPluginLogging',
        '../../statsreport/statsreport.gyp:o3dStatsReport',
      ],
      'include_dirs': [
        '../..',
        '../../..',
        '../../<(glewdir)/include',
        '../../<(cgdir)/include',
        '<(INTERMEDIATE_DIR)',
        '$(DXSDK_DIR)/Include',
      ],
      'defines': [
        'NOMINMAX',
        'WIN32',
        'WIN32_LEAN_AND_MEAN',
        '_ATL_SECURE_NO_WARNINGS',
        '_CRT_SECURE_NO_DEPRECATE',
        '_UNICODE', # turn on unicode
        '_WIN32_WINNT=0x0600',
        '_WINDOWS',
      ],
      'libraries': [
        '-ladvapi32.lib',
        '"$(DXSDK_DIR)/Lib/x86/dxguid.lib"',
        '-lmsi.lib',
        '-lole32.lib',
        '-loleaut32.lib',
        '-lshell32.lib',
        '-lshlwapi.lib',
        '-luser32.lib',
      ],
      # Disable the #pragma deprecated warning because
      # ATL seems to use deprecated CRT libs.
      'msvs_disabled_warnings': [4995],
      'msvs_configuration_attributes': {
        'UseOfATL': '1', # 1 = static link to ATL, 2 = dynamic link
      },
    },
    {
      'target_name': 'installer',
      'type': 'none',
      'variables': {
        'candle_exe': '../../../<(wixdir)/candle.exe',
        'light_exe': '../../../<(wixdir)/light.exe',
        'custom_actions_path': '<(PRODUCT_DIR)/cactions.dll',
        'd3dx_guid': '<!(<(guidgen) <(o3d_namespace_guid) d3dx-<(nppversion))',
        'dbl_path': '../../installer/win/driver_blacklist.txt',
        'dx_redist_guid': '<!(<(guidgen) <(o3d_namespace_guid) '
                          'dx_redist-<(nppversion))',
        'get_extras_path': '<(PRODUCT_DIR)/getextras.exe',
        'ieplugin_path': '<(PRODUCT_DIR)/o3d_host.dll',
        'include_software_renderer':
            '<!(python ../../build/file_exists.py '
            '../../../<(swiftshaderdir)/swiftshader_d3d9.dll)',
        'npplugin_path': '<(PRODUCT_DIR)/npo3dautoplugin.dll',
        'o3d_driver_blacklist_guid': '<!(<(guidgen) <(o3d_namespace_guid) '
                                     'o3d_driver_blacklist-<(nppversion))',
        'o3d_get_extras_guid':
            '<!(<(guidgen) <(o3d_namespace_guid) extras_installer-)',
        'o3d_iep_component_guid': '<!(<(guidgen) <(o3d_namespace_guid) '
                                  'o3d_ieplugin_component-<(nppversion))',
        'o3d_npp_component_guid': '<!(<(guidgen) <(o3d_namespace_guid) '
                                  'o3d_npplugin_component-<(nppversion))',
        'o3d_npp_google_update_reg_component_guid':
            '<!(<(guidgen) <(o3d_namespace_guid) '
            'o3d_user_google_update_reg_component-<(nppversion))',
        'o3d_npp_package_guid': '<!(<(guidgen) <(o3d_namespace_guid) o3d_package-<(nppversion))',
        'o3d_npp_product_guid': '<!(<(guidgen) <(o3d_namespace_guid) o3d_product-<(nppversion))',
        'o3d_npp_reg_key':
            '<(google_update_reg_path)<(o3d_npp_google_update_guid)',
        'o3d_npp_state_reg_key':
            '<(google_update_state_reg_path)<(o3d_npp_google_update_guid)',
        'o3d_reporter_guid':
            '<!(<(guidgen) <(o3d_namespace_guid) o3d_reporter-<(nppversion))',
        'o3d_software_renderer_guid': '<!(<(guidgen) <(o3d_namespace_guid) '
                                      'o3d_software_renderer-<(nppversion))',
        'rep_path': '<(PRODUCT_DIR)/reporter.exe',
        'software_renderer_path':
            '../../../<(swiftshaderdir)/swiftshader_d3d9.dll',
      },
      'dependencies': [
        '../../converter/converter.gyp:o3dConverter',
        '../../breakpad/breakpad.gyp:reporter',
        '../../google_update/google_update.gyp:getextras',
        '../../documentation/documentation.gyp:*',
        '../../plugin/plugin.gyp:npo3dautoplugin',
        '../../plugin/plugin.gyp:o3d_host',
        '../../samples/samples.gyp:samples',
        '../../build/libs.gyp:cg_libs',
        '../../build/libs.gyp:gl_libs',
        'cactions',
      ],
      'rules': [
        {
          'rule_name': 'candle',
          'extension': 'wxs',
          'process_outputs_as_sources': 1,
          'inputs': [
            '<(candle_exe)',
            '../../installer/win/docs.url',
            '<(PRODUCT_DIR)/cactions.dll',
            '<(PRODUCT_DIR)/cg.dll',
            '<(PRODUCT_DIR)/cgGL.dll',
            '<(PRODUCT_DIR)/cgc.exe',
            '<(PRODUCT_DIR)/o3dConditioner.exe',
            '<(dbl_path)',
            '<(get_extras_path)',
            '<(ieplugin_path)',
            '<(npplugin_path)',
            '<(rep_path)',
          ],
          'outputs': [
            '<(RULE_INPUT_ROOT).wixobj',
          ],
          'action': [
            '<(candle_exe)',
            '-nologo',
            '-dCustomActionsPath=<(custom_actions_path)',
            '-dD3DXGuid=<(d3dx_guid)',
            '-dDBLGuid=<(o3d_driver_blacklist_guid)',
            '-dDBLPath=<(dbl_path)',
            '-dDeprecatedUpgradeCode=<(bad_old_o3d_upgrade_code)',
            '-dGetExtrasGuid=<(o3d_get_extras_guid)',
            '-dGetExtrasPath=<(get_extras_path)',
            '-dIEPluginPath=<(ieplugin_path)',
            '-dIepComponentGuid=<(o3d_iep_component_guid)',
            '-dIncludeSoftwareRenderer=include_software_renderer',
            '-dNPPluginPath=<(npplugin_path)',
            '-dNppComponentGuid=<(o3d_npp_component_guid)',
            '-dNppGoogleUpdateRegGuid=<(o3d_npp_google_update_reg_component_guid)',
            '-dNppGoogleUpdateRegKey=<(o3d_npp_reg_key)',
            '-dNppGoogleUpdateStateRegKey=<(o3d_npp_state_reg_key)',
            '-dNppPackageGuid=<(o3d_npp_package_guid)',
            '-dNppProductGuid=<(o3d_npp_product_guid)',
            '-dNppUpgradeCode=<(o3d_npp_upgrade_code)',
            '-dNppVersion=<(dotnppversion)',
            '-dRepGuid=<(o3d_reporter_guid)',
            '-dRepPath=<(rep_path)',
            '-dSoftwareRendererGuid=<(o3d_software_renderer_guid)',
            '-dSoftwareRendererPath=<(software_renderer_path)',
            '-o',
            '<(RULE_INPUT_ROOT).wixobj',
            '<(RULE_INPUT_PATH)',
          ],
          'message': 'Generating installer from <(RULE_INPUT_PATH)',
        },
        {
          'rule_name': 'light',
          'extension': 'wixobj',
          'process_outputs_as_sources': 1,
          'inputs': [
            '<(light_exe)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/<(RULE_INPUT_ROOT).msi',
          ],
          'action': [
            '<(light_exe)',
            '-nologo',
            '-out',
            '<(PRODUCT_DIR)/<(RULE_INPUT_ROOT).msi',
            '<(RULE_INPUT_PATH)',
          ],
          'message': 'Linking installer from <(RULE_INPUT_PATH)',
        },
      ],
      'sources': [
        'o3d.wxs',
      ],
    },
  ],
  'conditions': [
    ['"<(dx_redist_exists)" == "True"',
      {
        'targets': [
          {
            'target_name': 'extras_installer',
            'type': 'none',
            'variables': {
              'candle_exe': '../../../<(wixdir)/candle.exe',
              'light_exe': '../../../<(wixdir)/light.exe',
              'o3d_extras_d3dx_component_guid': '<!(<(guidgen) <(o3d_namespace_guid) '
              'o3d_extras_d3dx_component-<(nppversion))',
              'o3d_extras_package_guid': '<!(<(guidgen) <(o3d_namespace_guid) '
              'o3d_extras_package-<(extrasversion))',
              'o3d_extras_product_guid': '<!(<(guidgen) <(o3d_namespace_guid) '
              'o3d_extras_product-<(extrasversion))',
              'o3d_extras_reg_key':
              '<(google_update_reg_path)<(o3d_extras_google_update_guid)',
            },
            'rules': [
              {
                'rule_name': 'candle',
                'extension': 'wxs',
                'process_outputs_as_sources': 1,
                'inputs': [
                  '<(candle_exe)',
                ],
                'outputs': [
                  '<(RULE_INPUT_ROOT).wixobj',
                ],
                'action': [
                  '<(candle_exe)',
                  '-nologo',
                  '-dDxRedistPath=<(dx_redist_path)',
                  '-dExtrasD3DXComponentGuid=<(o3d_extras_d3dx_component_guid)',
                  '-dExtrasGoogleUpdateRegGuid=<(o3d_extras_google_update_guid)',
                  '-dExtrasGoogleUpdateRegKey=<(o3d_extras_reg_key)',
                  '-dExtrasPackageGuid=<(o3d_extras_package_guid)',
                  '-dExtrasProductGuid=<(o3d_extras_product_guid)',
                  '-dExtrasUpgradeCode=<(o3d_extras_upgrade_code)',
                  '-dExtrasVersion=<(dotextrasversion)',
                  '-o',
                  '<(RULE_INPUT_ROOT).wixobj',
                  '<(RULE_INPUT_PATH)',
                ],
                'message': 'Generating extras installer from <(RULE_INPUT_PATH)',
              },
              {
                'rule_name': 'light',
                'extension': 'wixobj',
                'process_outputs_as_sources': 1,
                'inputs': [
                  '<(light_exe)',
                ],
                'outputs': [
                  '<(PRODUCT_DIR)/<(RULE_INPUT_ROOT).msi',
                ],
                'action': [
                  '<(light_exe)',
                  '-nologo',
                  '-out',
                  '<(PRODUCT_DIR)/<(RULE_INPUT_ROOT).msi',
                  '<(RULE_INPUT_PATH)',
                ],
                'message': 'Linking extras installer from <(RULE_INPUT_PATH)',
              },
            ],
            'sources': [
              'o3dextras.wxs',
            ],
          },
        ],
      },
      {
        'targets': [
          {
            'target_name': 'extras_installer',
            'type': 'none',
          },
        ],
      },
    ],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
