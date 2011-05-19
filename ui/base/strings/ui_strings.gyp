{
  'variables': {
    'chromium_code': 1,
    'grit_base_out_dir': '<(SHARED_INTERMEDIATE_DIR)/app',
  },
  'targets': [
    {
      'target_name': 'ui_strings',
      'type': 'none',
      'msvs_guid': 'BC3C49A3-D061-4E78-84EE-742DA064DC46',
      'actions': [
        {
          'action_name': 'app_strings',
          'variables': {
            'grit_grd_file': 'app_strings.grd',
            'grit_out_dir': '<(grit_base_out_dir)/app_strings',
          },
          'includes': [ '../../../build/grit_action.gypi' ],
        },
        {
          'action_name': 'app_locale_settings',
          'variables': {
            'grit_grd_file': 'app_locale_settings.grd',
            'grit_out_dir': '<(grit_base_out_dir)/app_locale_settings',
          },
          'includes': [ '../../../build/grit_action.gypi' ],
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(grit_base_out_dir)/app_locale_settings',
          '<(grit_base_out_dir)/app_strings',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': ['../../../build/win/system.gyp:cygwin'],
        }],
      ],
    },
  ],
  'conditions': [
    ['os_posix == 1 and OS != "mac"', {
      'targets': [{
        'target_name': 'ui_unittest_strings',
        'type': 'none',
        'variables': {
          'repack_path': '<(DEPTH)/tools/data_pack/repack.py',
        },
        'actions': [
          {
            'action_name': 'repack_ui_unittest_strings',
            'variables': {
              'pak_inputs': [
                '<(grit_base_out_dir)/app_strings/app_strings_en-US.pak',
                '<(grit_base_out_dir)/app_locale_settings/app_locale_settings_en-US.pak',
              ],
            },
            'inputs': [
              '<(repack_path)',
              '<@(pak_inputs)',
            ],
            'outputs': [
              '<(PRODUCT_DIR)/app_unittests_strings/en-US.pak',
            ],
            'action': ['python', '<(repack_path)', '<@(_outputs)',
                       '<@(pak_inputs)'],
          },
        ],
        'copies': [
          {
            'destination': '<(PRODUCT_DIR)/app_unittests_strings',
            'files': [
              '<(grit_base_out_dir)/app_resources/app_resources.pak',
            ],
          },
        ],
      }],
    }],
  ],
}
