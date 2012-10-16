# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


trusted_scons_files = [
]


# Untrusted libraries for use by user code.
untrusted_scons_files = [
    'src/shared/ppapi/nacl.scons',
]


# Untrusted libraries for use by system code.
untrusted_irt_scons_files = [
]


nonvariant_test_scons_files = [
    'tests/breakpad_crash_test/nacl.scons',
    'tests/ppapi/nacl.scons',
    'tests/ppapi_browser/bad/nacl.scons',
    'tests/ppapi_browser/crash/nacl.scons',
    'tests/ppapi_browser/extension_mime_handler/nacl.scons',
    'tests/ppapi_browser/manifest/nacl.scons',
    'tests/ppapi_browser/ppb_audio/nacl.scons',
    'tests/ppapi_browser/ppb_dev/nacl.scons',
    'tests/ppapi_browser/ppb_file_system/nacl.scons',
    'tests/ppapi_browser/ppb_fullscreen/nacl.scons',
    'tests/ppapi_browser/ppb_graphics3d/nacl.scons',
    'tests/ppapi_browser/ppb_image_data/nacl.scons',
    'tests/ppapi_browser/ppb_instance/nacl.scons',
    'tests/ppapi_browser/ppb_var/nacl.scons',
    'tests/ppapi_browser/ppp_input_event/nacl.scons',
    'tests/ppapi_browser/ppp_instance/nacl.scons',
    'tests/ppapi_browser/progress_events/nacl.scons',
    'tests/ppapi_messaging/nacl.scons',
    'tests/ppapi_test_lib/nacl.scons',
]

