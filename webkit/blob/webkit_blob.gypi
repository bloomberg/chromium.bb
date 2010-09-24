# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'blob',
      'type': '<(library)',
      'msvs_guid': '02567509-F7CA-4E84-8524-4F72DA2D3111',
      'dependencies': [
        '<(DEPTH)/app/app.gyp:app_base',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/net/net.gyp:net',
      ],
      'sources': [
        'blob_data.cc',
        'blob_data.h',
        'blob_storage_controller.cc',
        'blob_storage_controller.h',
        'blob_url_request_job.cc',
        'blob_url_request_job.h',
      ],
      'conditions': [
        ['inside_chromium_build==0', {
          'dependencies': [
            '<(DEPTH)/webkit/support/setup_third_party.gyp:third_party_headers',
          ],
        }],
      ],
    },
  ],
}
