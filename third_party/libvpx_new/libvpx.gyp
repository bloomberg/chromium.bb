# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      # This libvpx target contains both encoder and decoder.
      # Encoder is configured to be realtime only.
      'target_name': 'libvpx_new',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/third_party/libvpx/libvpx.gyp:libvpx'
      ],
      'export_dependent_settings': [
        '<(DEPTH)/third_party/libvpx/libvpx.gyp:libvpx'
      ],
    },
  ],
}
