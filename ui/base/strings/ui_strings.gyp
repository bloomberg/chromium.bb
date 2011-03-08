{
  'variables': {
    # TODO: remove this helper when we have loops in GYP
    'apply_locales_cmd': ['python', '<(DEPTH)/build/apply_locales.py',],
    'chromium_code': 1,
    'grit_cmd': ['python', '../../../tools/grit/grit.py'],
    'grit_info_cmd': ['python', '../../../tools/grit/grit_info.py',
                      '<@(grit_defines)'],
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/app',
    'localizable_resources': [
      'app_locale_settings.grd',
      'app_strings.grd',
    ],
  },
  'targets': [
    {
      'target_name': 'ui_strings',
      'type': 'none',
      'msvs_guid': 'BC3C49A3-D061-4E78-84EE-742DA064DC46',
      'rules': [
        {
          'rule_name': 'grit',
          'extension': 'grd',
          'inputs': [
            '<!@(<(grit_info_cmd) --inputs <(localizable_resources))',
          ],
          'outputs': [
            '<(grit_out_dir)/<(RULE_INPUT_ROOT)/grit/<(RULE_INPUT_ROOT).h',
            # TODO: remove this helper when we have loops in GYP
            '>!@(<(apply_locales_cmd) \'<(grit_out_dir)/<(RULE_INPUT_ROOT)/<(RULE_INPUT_ROOT)_ZZLOCALE.pak\' <(locales))',
          ],
          'action': [
     '<@(grit_cmd)',
     '-i', '<(RULE_INPUT_PATH)', 'build',
     '-o', '<(grit_out_dir)/<(RULE_INPUT_ROOT)',
            '<@(grit_defines)'],
          'message': 'Generating resources from <(RULE_INPUT_PATH)',
        },
      ],
      'sources': [
        '<@(localizable_resources)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(grit_out_dir)/app_locale_settings',
          '<(grit_out_dir)/app_strings',
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
    ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
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
                '<(grit_out_dir)/app_strings/app_strings_en-US.pak',
                '<(grit_out_dir)/app_locale_settings/app_locale_settings_en-US.pak',
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
              '<(grit_out_dir)/app_resources/app_resources.pak',
            ],
          },
        ],
      }],
    }],
  ],
}
