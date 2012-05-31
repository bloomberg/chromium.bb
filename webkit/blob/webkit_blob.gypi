# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      ['inside_chromium_build==0', {
        'webkit_src_dir': '../../../../..',
      },{
        'webkit_src_dir': '../../third_party/WebKit',
      }],
      ],
    },
  'targets': [
    {
      'target_name': 'blob',
      'type': '<(component)',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(DEPTH)/net/net.gyp:net',
        '<(webkit_src_dir)/Source/WebKit/chromium/WebKit.gyp:webkit',
      ],
      'defines': [
        'BLOB_IMPLEMENTATION'
      ],
      'sources': [
        'blob_data.cc',
        'blob_data.h',
        'blob_storage_controller.cc',
        'blob_storage_controller.h',
        'blob_url_request_job.cc',
        'blob_url_request_job.h',
        'blob_url_request_job_factory.cc',
        'blob_url_request_job_factory.h',
        'local_file_stream_reader.cc',
        'local_file_stream_reader.h',
        'shareable_file_reference.cc',
        'shareable_file_reference.h',
        'view_blob_internals_job.cc',
        'view_blob_internals_job.h',
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
