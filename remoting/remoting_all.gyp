# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'remoting_all',
      'type': 'none',
      'dependencies': [
        '../remoting/remoting.gyp:*',
        '../remoting/app_remoting_webapp.gyp:*',
      ],
    },  # end of target 'remoting_all'
  ],  # end of targets
}
