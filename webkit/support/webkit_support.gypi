# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'webkit_support',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/skia/skia.gyp:skia',
        'appcache',
        'database',
        'glue',
      ],
      'sources': [
        'test_webkit_client.cc',
        'test_webkit_client.h',
        'test_webplugin_page_delegate.h',
        'webkit_support.cc',
        'webkit_support.h',
        'webkit_support_glue.cc',
        # TODO(tkent): Move the following files to here.
        '<(DEPTH)/webkit/tools/test_shell/mock_webclipboard_impl.cc',
        '<(DEPTH)/webkit/tools/test_shell/mock_webclipboard_impl.h',
        '<(DEPTH)/webkit/tools/test_shell/simple_appcache_system.cc',
        '<(DEPTH)/webkit/tools/test_shell/simple_appcache_system.h',
        '<(DEPTH)/webkit/tools/test_shell/simple_clipboard_impl.cc',
        '<(DEPTH)/webkit/tools/test_shell/simple_database_system.cc',
        '<(DEPTH)/webkit/tools/test_shell/simple_database_system.h',
        '<(DEPTH)/webkit/tools/test_shell/simple_resource_loader_bridge.cc',
        '<(DEPTH)/webkit/tools/test_shell/simple_resource_loader_bridge.h',
        '<(DEPTH)/webkit/tools/test_shell/simple_socket_stream_bridge.cc',
        '<(DEPTH)/webkit/tools/test_shell/simple_socket_stream_bridge.h',
        '<(DEPTH)/webkit/tools/test_shell/simple_webcookiejar_impl.cc',
        '<(DEPTH)/webkit/tools/test_shell/simple_webcookiejar_impl.h',
        '<(DEPTH)/webkit/tools/test_shell/test_shell_request_context.cc',
        '<(DEPTH)/webkit/tools/test_shell/test_shell_request_context.h',
      ],
    },
  ],
}
# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
