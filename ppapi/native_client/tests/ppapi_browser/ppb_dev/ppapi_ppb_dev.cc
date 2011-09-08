// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Test cases for PPAPI Dev interfaces.
//

#include <string.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/plugin_nacl_file.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_core.h"

#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/c/dev/ppb_console_dev.h"
#include "ppapi/c/dev/ppb_context_3d_dev.h"
#include "ppapi/c/dev/ppb_context_3d_trusted_dev.h"
#include "ppapi/c/dev/ppb_crypto_dev.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/dev/ppb_directory_reader_dev.h"
#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/c/dev/ppb_find_dev.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/dev/ppb_fullscreen_dev.h"
#include "ppapi/c/dev/ppb_gles_chromium_texture_mapping_dev.h"
#include "ppapi/c/dev/ppb_layer_compositor_dev.h"
#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/c/dev/ppb_mouse_lock_dev.h"
#include "ppapi/c/dev/ppb_scrollbar_dev.h"
#include "ppapi/c/dev/ppb_surface_3d_dev.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/dev/ppb_transport_dev.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/dev/ppb_video_decoder_dev.h"
#include "ppapi/c/dev/ppb_widget_dev.h"
#include "ppapi/c/dev/ppb_zoom_dev.h"


namespace {

////////////////////////////////////////////////////////////////////////////////
// Test Cases
////////////////////////////////////////////////////////////////////////////////

void TestGetDevInterfaces() {
  // This test is run only w/ NACL_ENABLE_PPAPI_DEV=0, which should
  // turn off and disable the PPAPI developer interfaces. When they are
  // disabled, the interface should return NULL.
  CHECK(GetBrowserInterface(PPB_BUFFER_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_CHAR_SET_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_CONSOLE_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_CONTEXT_3D_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_CRYPTO_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_CURSOR_CONTROL_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_DIRECTORYREADER_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_FILECHOOSER_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_FIND_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_FONT_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_FULLSCREEN_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(
      PPB_GLES_CHROMIUM_TEXTURE_MAPPING_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_LAYER_COMPOSITOR_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_MEMORY_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_MOUSELOCK_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_SCROLLBAR_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_SURFACE_3D_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_TESTING_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_TRANSPORT_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_URLUTIL_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_VAR_DEPRECATED_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_VIDEODECODER_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_WIDGET_DEV_INTERFACE) == NULL);
  CHECK(GetBrowserInterface(PPB_ZOOM_DEV_INTERFACE) == NULL);

  TEST_PASSED;
}

}  // namespace

void SetupTests() {
  RegisterTest("TestGetDevInterfaces", TestGetDevInterfaces);
}

void SetupPluginInterfaces() {
  // none
}
