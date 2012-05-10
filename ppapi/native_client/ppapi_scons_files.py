# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


trusted_scons_files = [
    'src/shared/ppapi/build.scons',
    'src/shared/ppapi_proxy/build.scons',
    'src/trusted/plugin/build.scons',
    'tests/ppapi_geturl/build.scons',
    'tests/ppapi_messaging/build.scons',
    'tests/ppapi_browser/ppb_file_system/build.scons',
    'tests/ppapi_tests/build.scons',  # Build PPAPI tests from Chrome as a .so
]


# Untrusted libraries for use by user code.
untrusted_scons_files = [
    'src/shared/ppapi/nacl.scons',
]


# Untrusted libraries for use by system code.
untrusted_irt_scons_files = [
    'src/shared/ppapi_proxy/nacl.scons',
]


nonvariant_test_scons_files = [
    'tests/ppapi/nacl.scons',
    'tests/ppapi_browser/bad/nacl.scons',
    'tests/ppapi_browser/crash/nacl.scons',
    'tests/ppapi_browser/extension_mime_handler/nacl.scons',
    'tests/ppapi_browser/manifest/nacl.scons',
    'tests/ppapi_browser/ppb_core/nacl.scons',
    'tests/ppapi_browser/ppb_dev/nacl.scons',
    'tests/ppapi_browser/ppb_file_system/nacl.scons',
    'tests/ppapi_browser/ppb_graphics2d/nacl.scons',
    'tests/ppapi_browser/ppb_graphics3d/nacl.scons',
    'tests/ppapi_browser/ppb_image_data/nacl.scons',
    'tests/ppapi_browser/ppb_instance/nacl.scons',
    'tests/ppapi_browser/ppb_memory/nacl.scons',
    'tests/ppapi_browser/ppb_pdf/nacl.scons',
    'tests/ppapi_browser/ppb_scrollbar/nacl.scons',
    'tests/ppapi_browser/ppb_url_loader/nacl.scons',
    'tests/ppapi_browser/ppb_url_request_info/nacl.scons',
    'tests/ppapi_browser/ppb_var/nacl.scons',
    'tests/ppapi_browser/ppb_widget/nacl.scons',
    'tests/ppapi_browser/ppp_input_event/nacl.scons',
    'tests/ppapi_browser/ppp_instance/nacl.scons',
    'tests/ppapi_browser/progress_events/nacl.scons',
    'tests/ppapi_browser/stress_many_nexes/nacl.scons',
    'tests/ppapi_example_2d/nacl.scons',
    'tests/ppapi_example_audio/nacl.scons',
    'tests/ppapi_example_events/nacl.scons',
    # TODO(dspringer): re-enable test once the 3D ABI has stabilized. See
    # http://code.google.com/p/nativeclient/issues/detail?id=2060
    # 'tests/ppapi_example_gles2/nacl.scons',
    'tests/ppapi_example_post_message/nacl.scons',
    'tests/ppapi_geturl/nacl.scons',
    'tests/ppapi_gles_book/nacl.scons',
    'tests/ppapi_messaging/nacl.scons',
    # Broken by Chrome change
    # http://code.google.com/p/nativeclient/issues/detail?id=2480
    #'tests/ppapi_simple_tests/nacl.scons',
    'tests/ppapi_test_example/nacl.scons',
    'tests/ppapi_test_lib/nacl.scons',
    'tests/ppapi_tests/nacl.scons',
]

