# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'appcache',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines': [
        'APPCACHE_IMPLEMENTATION',
      ],
      'dependencies': [
        'quota',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/sql/sql.gyp:sql',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      ],
      'sources': [
        # This list contains all .h and .cc in appcache except for test code.
        'appcache.cc',
        'appcache.h',
        'appcache_backend_impl.cc',
        'appcache_backend_impl.h',
        'appcache_database.cc',
        'appcache_database.h',
        'appcache_disk_cache.cc',
        'appcache_disk_cache.h',
        'appcache_entry.h',
        'appcache_export.h',
        'appcache_frontend_impl.cc',
        'appcache_frontend_impl.h',
        'appcache_group.cc',
        'appcache_group.h',
        'appcache_histograms.cc',
        'appcache_histograms.h',
        'appcache_host.cc',
        'appcache_host.h',
        'appcache_interceptor.cc',
        'appcache_interceptor.h',
        'appcache_interfaces.cc',
        'appcache_interfaces.h',
        'appcache_policy.h',
        'appcache_quota_client.cc',
        'appcache_quota_client.h',
        'appcache_request_handler.cc',
        'appcache_request_handler.h',
        'appcache_response.cc',
        'appcache_response.h',
        'appcache_service.cc',
        'appcache_service.h',
        'appcache_storage.cc',
        'appcache_storage.h',
        'appcache_storage_impl.cc',
        'appcache_storage_impl.h',
        'appcache_working_set.cc',
        'appcache_working_set.h',
        'appcache_update_job.cc',
        'appcache_update_job.h',
        'appcache_url_request_job.cc',
        'appcache_url_request_job.h',
        'manifest_parser.cc',
        'manifest_parser.h',
        'view_appcache_internals_job.h',
        'view_appcache_internals_job.cc',
        'web_application_cache_host_impl.cc',
        'web_application_cache_host_impl.h',
        'webkit_appcache.gypi',
      ],
      'conditions': [
        [# TODO(dpranke): Remove once the circular dependencies in
         # WebKit.gyp are fixed on the mac.
         # See https://bugs.webkit.org/show_bug.cgi?id=68463
         'OS=="mac"', {
          'type': 'static_library',
         }, {
          'type': '<(component)',
          'dependencies': [
              '<(DEPTH)/third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit',
          ],
         }],
        ['inside_chromium_build==0', {
          'dependencies': [
            '<(DEPTH)/webkit/support/setup_third_party.gyp:third_party_headers',
          ],
        }],
      ],
    },
  ],
}
