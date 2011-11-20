// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implements the untrusted side of the PPB_GetInterface method.

#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include <stdlib.h>
#include <string.h>
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_audio.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_audio_config.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_buffer.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_core.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_cursor_control.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_file_io.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_file_ref.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_file_system.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_find.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_font.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_fullscreen.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_graphics_2d.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_graphics_3d.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_image_data.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_input_event.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_instance.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_memory.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_messaging.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_mouse_lock.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_pdf.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_scrollbar.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_testing.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_url_loader.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_url_request_info.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_url_response_info.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_var.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_widget.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_zoom.h"
#include "native_client/src/shared/ppapi_proxy/untrusted/srpcgen/ppb_rpc.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"

namespace ppapi_proxy {

namespace {

struct InterfaceMapElement {
  const char* name;
  const void* ppb_interface;
  bool needs_browser_check;
};

// List of interfaces that have at least partial proxy support.
InterfaceMapElement interface_map[] = {
  { PPB_AUDIO_INTERFACE, PluginAudio::GetInterface(), true },
  { PPB_AUDIO_CONFIG_INTERFACE, PluginAudioConfig::GetInterface(), true },
  { PPB_CORE_INTERFACE, PluginCore::GetInterface(), true },
  { PPB_CURSOR_CONTROL_DEV_INTERFACE, PluginCursorControl::GetInterface(),
    true },
  { PPB_FILEIO_INTERFACE, PluginFileIO::GetInterface(), true },
  { PPB_FILEREF_INTERFACE, PluginFileRef::GetInterface(), true },
  { PPB_FILESYSTEM_INTERFACE, PluginFileSystem::GetInterface(), true },
  { PPB_FIND_DEV_INTERFACE, PluginFind::GetInterface(), true },
  { PPB_FONT_DEV_INTERFACE, PluginFont::GetInterface(), true },
  { PPB_FULLSCREEN_INTERFACE, PluginFullscreen::GetInterface(), true },
  { PPB_GRAPHICS_2D_INTERFACE, PluginGraphics2D::GetInterface(), true },
  { PPB_GRAPHICS_3D_INTERFACE, PluginGraphics3D::GetInterface(), true },
  { PPB_IMAGEDATA_INTERFACE, PluginImageData::GetInterface(), true },
  { PPB_INPUT_EVENT_INTERFACE, PluginInputEvent::GetInterface(), true },
  { PPB_INSTANCE_INTERFACE, PluginInstance::GetInterface(), true },
  { PPB_KEYBOARD_INPUT_EVENT_INTERFACE,
    PluginInputEvent::GetKeyboardInterface(), true },
  { PPB_MEMORY_DEV_INTERFACE, PluginMemory::GetInterface(), true },
  { PPB_MESSAGING_INTERFACE, PluginMessaging::GetInterface(), true },
  { PPB_MOUSE_INPUT_EVENT_INTERFACE_1_0,
    PluginInputEvent::GetMouseInterface1_0(), true },
  { PPB_MOUSE_INPUT_EVENT_INTERFACE_1_1,
    PluginInputEvent::GetMouseInterface1_1(), true },
  { PPB_MOUSELOCK_INTERFACE, PluginMouseLock::GetInterface(), true },
  { PPB_OPENGLES2_INTERFACE, PluginGraphics3D::GetOpenGLESInterface(),
    true },
  { PPB_PDF_INTERFACE, PluginPDF::GetInterface(), true },
  { PPB_SCROLLBAR_DEV_INTERFACE, PluginScrollbar::GetInterface(), true },
  { PPB_TESTING_DEV_INTERFACE_0_7, PluginTesting::GetInterface(), true },
  { PPB_TESTING_DEV_INTERFACE_0_8, PluginTesting::GetInterface(), true },
  { PPB_URLLOADER_INTERFACE, PluginURLLoader::GetInterface(), true },
  { PPB_URLREQUESTINFO_INTERFACE, PluginURLRequestInfo::GetInterface(), true },
  { PPB_URLRESPONSEINFO_INTERFACE, PluginURLResponseInfo::GetInterface(),
    true },
  { PPB_VAR_INTERFACE, PluginVar::GetInterface(), true },
  { PPB_WHEEL_INPUT_EVENT_INTERFACE, PluginInputEvent::GetWheelInterface(),
    true },
  { PPB_WIDGET_DEV_INTERFACE, PluginWidget::GetInterface(), true },
  { PPB_ZOOM_DEV_INTERFACE, PluginZoom::GetInterface(), true },
};

}  // namespace

// Returns the pointer to the interface proxy or NULL if not yet supported.
// On the first invocation for a given interface that has proxy support,
// also confirms over RPC that the browser indeed exports this interface.
// Returns NULL otherwise.
const void* GetBrowserInterface(const char* interface_name) {
  DebugPrintf("PPB_GetInterface('%s')\n", interface_name);
  const void* ppb_interface = NULL;
  int index = -1;
  // The interface map is so small that anything but a linear search would be an
  // overkill, especially since the keys are macros and might not sort in any
  // obvious order relative to the name.
  for (size_t i = 0; i < NACL_ARRAY_SIZE(interface_map); ++i) {
    if (strcmp(interface_name, interface_map[i].name) == 0) {
      ppb_interface = interface_map[i].ppb_interface;
      index = i;
      break;
    }
  }
  DebugPrintf("PPB_GetInterface('%s'): %p\n", interface_name, ppb_interface);
  if (index == -1 || ppb_interface == NULL)
    return NULL;
  if (!interface_map[index].needs_browser_check)
    return ppb_interface;

  int32_t browser_exports_interface;
  NaClSrpcError srpc_result =
      PpbRpcClient::PPB_GetInterface(GetMainSrpcChannel(),
                                     const_cast<char*>(interface_name),
                                     &browser_exports_interface);
  DebugPrintf("PPB_GetInterface('%s'): %s\n",
              interface_name, NaClSrpcErrorString(srpc_result));
  if (srpc_result != NACL_SRPC_RESULT_OK || !browser_exports_interface) {
    interface_map[index].ppb_interface = NULL;
    ppb_interface = NULL;
  }
  interface_map[index].needs_browser_check = false;
  return ppb_interface;
}

}  // namespace ppapi_proxy
