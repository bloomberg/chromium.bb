# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS=="ios"', {
      'variables': {
        'remoting_ios_locales': [
          'ar', 'ca', 'cs', 'da', 'de', 'el', 'en', 'en-GB', 'es', 'es-MX',
          'fi', 'fr', 'he', 'hi', 'hr', 'hu', 'id', 'it', 'ja', 'ko', 'ms',
          'nb', 'nl', 'pl', 'pt', 'pt-PT', 'ro', 'ru', 'sk', 'sv', 'th', 'tr',
          'uk', 'vi', 'zh-CN', 'zh-TW',
        ],
        'locales_out_dir': '<(SHARED_INTERMEDIATE_DIR)/remoting/ios/resources',
        'remoting_ios_locale_files': [
          '<!@pymod_do_main(remoting_ios_localize --print-outputs '
              ' --to-dir <(locales_out_dir) '
              '<(remoting_ios_locales))',
        ],
        'remoting_ios_credits_files': [
          '<(SHARED_INTERMEDIATE_DIR)/remoting/ios/credits',
        ],
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/remoting/ios/grit',
      },  # variables

      'targets': [
        {
          'target_name': 'remoting_ios_l10n',
          'type': 'none',
          'variables': {
            'string_id_list': 'client/ios/build/localizable_string_id_list.txt',
            'infoplist_template': 'client/ios/build/InfoPlist.strings.jinja2',
          },
          'actions': [
            {
              'action_name': 'Generate <locale>.pak from remoting_strings.grd',
              'variables': {
                'grit_grd_file': 'resources/remoting_strings.grd',
                'grit_whitelist': '',
                'grit_additional_defines': [
                  '-D', '_google_chrome'
                ],
              },
              'includes': [ '../build/grit_action.gypi' ],
            },
            {
              'action_name': 'Generate Localizable.strings and InfoPlist.strings for each locale',
              'inputs': [
                'tools/build/remoting_ios_localize.py',
                '<(string_id_list)',
                '<(infoplist_template)',
                '<!@pymod_do_main(remoting_ios_localize --print-inputs '
                    '--from-dir <(grit_out_dir)/remoting/resources '
                    '<(remoting_ios_locales))',
              ],
              'outputs': [
                '<@(remoting_ios_locale_files)'
              ],
              'action': [
                'python', 'tools/build/remoting_ios_localize.py',
                '--from-dir', '<(grit_out_dir)/remoting/resources',
                '--to-dir', '<(locales_out_dir)',
                '--localizable-list', '<(string_id_list)',
                '--infoplist-template', '<(infoplist_template)',
                '--resources-header', '<(grit_out_dir)/remoting/base/string_resources.h',
                '<@(remoting_ios_locales)',
              ],
            },
          ],
          # Copy string_resources.h from gen/remoting/ios/grit/remoting/resources
          # to gen/remoting/ios for a nicer include path.
          'copies': [
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/remoting/ios',
              'files': [
                '<(grit_out_dir)/remoting/base/string_resources.h',
              ],
            },
          ],
        },  # end of target 'remoting_ios_l10n'

        {
          'target_name': 'remoting_ios_credits',
          'type': 'shared_library',
          'product_extension': 'bundle',
          'mac_bundle': 1,
          'mac_bundle_resources': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/credits.html',
            'webapp/base/html/credits_css.css',
            'webapp/base/html/main.css',
            'webapp/base/js/credits_js.js',
          ],
          'dependencies': [
            'remoting_client_credits',
          ],
        },  # end of target remoting_ios_credits
        
      ],  # end of 'targets'
    }],  # 'OS=="ios"
  ],  # end of 'conditions'
}
