# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'compile_all_resources',
      'type': 'none',
      'dependencies': [
        '../../chrome/browser/resources/downloads/compiled_resources.gyp:*',
        '../../chrome/browser/resources/history/compiled_resources.gyp:*',
        '../../chrome/browser/resources/uber/compiled_resources.gyp:*',
        '../../ui/webui/resources/js/chromeos/compiled_resources.gyp:*',
        '../../ui/webui/resources/js/compiled_resources.gyp:*',
        '../../ui/webui/resources/js/cr/ui/compiled_resources.gyp:*',
      ]
    },
  ]
}
