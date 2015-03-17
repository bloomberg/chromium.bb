# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO: Factor out all of the common items across the test targets into a
# single .gypi file that can be included by each test target.

{
  'conditions': [
    ['archive_chromoting_tests==1', {
      'targets': [
        {
          'target_name': 'chromoting_integration_tests_run',
          'type': 'none',
          'dependencies': [
            '../../chrome/chrome.gyp:browser_tests',
            '../../remoting/remoting.gyp:remoting_webapp_v1',
            '../../remoting/remoting.gyp:remoting_webapp_v2',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            'chromoting_integration_tests.isolate',
          ],
          'conditions': [
            ['OS=="linux"', {
              'dependencies': [
                '../../remoting/remoting.gyp:remoting_me2me_host_archive',
              ],
            }],  # OS=="linux"
          ],
        },  # target_name: 'chromoting_integration_tests_run'
        {
          'target_name': 'chromoting_multi_machine_example_test',
          'type': 'none',
          'dependencies': [
            '../../chrome/chrome.gyp:browser_tests',
            '../../remoting/remoting.gyp:remoting_webapp_v1',
            '../../remoting/remoting.gyp:remoting_webapp_v2',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            'multi_machine_example/example_test_controller.isolate',
            'multi_machine_example/example_task.isolate',
          ],
          'conditions': [
            ['OS=="linux"', {
              'dependencies': [
                '../../remoting/remoting.gyp:remoting_me2me_host_archive',
              ],
            }],  # OS=="linux"
          ],
        },  # target_name: 'chromoting_multi_machine_example_test'
      ],
    }],
  ],
}
