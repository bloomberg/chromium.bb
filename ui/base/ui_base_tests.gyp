# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # TODO(tfarina): Remove this target after all traces of it are updated to
      # point to ui_base_unittests. That means updating buildbot code and some
      # references in chromium too. crbug.com/331829
      # GN version: //ui/base:unittests
      'target_name': 'ui_unittests',
      'includes': [ 'ui_base_tests.gypi' ],
    },
    {
      # GN version: //ui/base:unittests
      'target_name': 'ui_base_unittests',
      # TODO(tfarina): When ui_unittests is removed, move the content of the
      # gypi file back here.
      'includes': [ 'ui_base_tests.gypi' ],
    },
  ],
  'conditions': [
    # Mac target to build a test Framework bundle to mock out resource loading.
    ['OS == "mac"', {
      'targets': [
        {
          'target_name': 'ui_base_tests_bundle',
          'type': 'shared_library',
          'dependencies': [
            '../resources/ui_resources.gyp:ui_test_pak',
          ],
          'includes': [ 'ui_base_tests_bundle.gypi' ],
        },
      ],
    }],
    ['OS == "android"', {
      'targets': [
        {
          # TODO(tfarina): Remove this target after all traces of it are updated
          # to point to ui_base_unittests_apk. crbug.com/331829
          'target_name': 'ui_unittests_apk',
          'type': 'none',
          'dependencies': [
            'ui_unittests',
          ],
          'variables': {
            'test_suite_name': 'ui_unittests',
          },
          'includes': [ '../../build/apk_test.gypi' ],
        },
        {
          'target_name': 'ui_base_unittests_apk',
          'type': 'none',
          'dependencies': [
            'ui_base_unittests',
          ],
          'variables': {
            'test_suite_name': 'ui_base_unittests',
          },
          'includes': [ '../../build/apk_test.gypi' ],
        },
      ],
    }],
  ],
}
