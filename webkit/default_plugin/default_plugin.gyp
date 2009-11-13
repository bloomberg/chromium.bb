# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'default_plugin',
          'type': '<(library)',
          'dependencies': [
            '../../net/net.gyp:net_resources',
            '../../third_party/icu/icu.gyp:icui18n',
            '../../third_party/icu/icu.gyp:icuuc',
            '../../third_party/libxml/libxml.gyp:libxml',
            '../../third_party/npapi/npapi.gyp:npapi',
            '../webkit.gyp:webkit_resources',
            '../webkit.gyp:webkit_strings',
          ],
          'include_dirs': [
            '../..',
            '../../chrome/third_party/wtl/include',
            # TODO(bradnelson): this should fall out of the dependencies.
            '<(SHARED_INTERMEDIATE_DIR)/webkit',
          ],
          'msvs_guid': '5916D37D-8C97-424F-A904-74E52594C2D6',
          'sources': [
            'default_plugin.cc',
            'default_plugin_resources.h',
            'default_plugin_shared.h',
            'install_dialog.cc',
            'install_dialog.h',
            'plugin_database_handler.cc',
            'plugin_database_handler.h',
            'plugin_impl_win.cc',
            'plugin_impl_win.h',
            'plugin_install_job_monitor.cc',
            'plugin_install_job_monitor.h',
            'plugin_main.cc',
            'plugin_main.h',
          ],
          'link_settings': {
            'libraries': ['-lurlmon.lib'],
          },
        },
      ],
    },],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
