# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'fileapi',
      'type': '<(library)',
      'msvs_guid': '40B53211-03ED-4932-8D53-52B172599DFE',
      'dependencies': [
        '<(DEPTH)/app/app.gyp:app_base',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/net/net.gyp:net',
      ],
      'sources': [
        'file_system_callback_dispatcher.h',
        'file_system_context.cc',
        'file_system_context.h',
        'file_system_dir_url_request_job.cc',
        'file_system_dir_url_request_job.h',
        'file_system_file_util.cc',
        'file_system_file_util.h',
        'file_system_file_util_proxy.cc',
        'file_system_file_util_proxy.h',
        'file_system_operation.cc',
        'file_system_operation.h',
        'file_system_path_manager.cc',
        'file_system_path_manager.h',
        'file_system_types.h',
        'file_system_url_request_job.cc',
        'file_system_url_request_job.h',
        'file_system_usage_tracker.cc',
        'file_system_usage_tracker.h',
        'file_system_util.cc',
        'file_system_util.h',
        'file_system_usage_cache.h',
        'file_system_usage_cache.cc',
        'file_writer_delegate.cc',
        'file_writer_delegate.h',
        'webfilewriter_base.cc',
        'webfilewriter_base.h',
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
