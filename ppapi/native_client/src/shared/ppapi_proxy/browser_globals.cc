// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_globals.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "ppapi/c/dev/ppb_gles_chromium_texture_mapping_dev.h"
#include "ppapi/c/dev/ppb_layer_compositor_dev.h"
#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/c/trusted/ppb_graphics_3d_trusted.h"

namespace ppapi_proxy {

// All of these methods are called from the browser main (UI, JavaScript, ...)
// thread.

const PP_Resource kInvalidResourceId = 0;

namespace {

std::map<PP_Instance, BrowserPpp*>* instance_to_ppp_map = NULL;

// In the future, we might have one sel_ldr with one channel per module, shared
// by multiple instances, where each instance consists of a trusted plugin with
// a proxy to a nexe. When this happens, we will need to provide the instance id
// with each SRPC message. But in the meantime it is enough to map channels
// to instances since each proxy has its own SRPC channel.
std::map<NaClSrpcChannel*, PP_Module>* channel_to_module_id_map = NULL;
std::map<NaClSrpcChannel*, PP_Instance>* channel_to_instance_id_map = NULL;

// The function pointer from the browser, and whether or not the plugin
// is requesting PPAPI Dev interfaces to be available.
// Set by SetPPBGetInterface().
PPB_GetInterface get_interface = NULL;
bool enable_dev_interfaces = false;

// Whether Pepper 3D interfaces should be enabled.
bool enable_3d_interfaces = true;

}  // namespace

void SetBrowserPppForInstance(PP_Instance instance, BrowserPpp* browser_ppp) {
  // If there was no map, create one.
  if (instance_to_ppp_map == NULL) {
    instance_to_ppp_map = new std::map<PP_Instance, BrowserPpp*>;
  }
  // Add the instance to the map.
  (*instance_to_ppp_map)[instance] = browser_ppp;
}

void UnsetBrowserPppForInstance(PP_Instance instance) {
  if (instance_to_ppp_map == NULL) {
    // Something major is wrong here.  We are deleting a map entry
    // when there is no map.
    NACL_NOTREACHED();
    return;
  }
  // Erase the instance from the map.
  instance_to_ppp_map->erase(instance);
  // If there are no more instances alive, remove the map.
  if (instance_to_ppp_map->size() == 0) {
    delete instance_to_ppp_map;
    instance_to_ppp_map = NULL;
  }
}

BrowserPpp* LookupBrowserPppForInstance(PP_Instance instance) {
  if (instance_to_ppp_map == NULL) {
    return NULL;
  }
  return (*instance_to_ppp_map)[instance];
}

void SetModuleIdForSrpcChannel(NaClSrpcChannel* channel, PP_Module module_id) {
  // If there was no map, create one.
  if (channel_to_module_id_map == NULL) {
    channel_to_module_id_map = new std::map<NaClSrpcChannel*, PP_Module>;
  }
  // Add the channel to the map.
  (*channel_to_module_id_map)[channel] = module_id;
}

void SetInstanceIdForSrpcChannel(NaClSrpcChannel* channel,
                                 PP_Instance instance_id) {
  if (channel_to_instance_id_map == NULL) {
    channel_to_instance_id_map = new std::map<NaClSrpcChannel*, PP_Instance>;
  }
  (*channel_to_instance_id_map)[channel] = instance_id;
}

void UnsetModuleIdForSrpcChannel(NaClSrpcChannel* channel) {
  if (channel_to_module_id_map == NULL) {
    // Something major is wrong here.  We are deleting a map entry
    // when there is no map.
    NACL_NOTREACHED();
    return;
  }
  // Erase the channel from the map.
  channel_to_module_id_map->erase(channel);
  // If there are no more channels alive, remove the map.
  if (channel_to_module_id_map->size() == 0) {
    delete channel_to_module_id_map;
    channel_to_module_id_map = NULL;
  }
}

void UnsetInstanceIdForSrpcChannel(NaClSrpcChannel* channel) {
  if (channel_to_instance_id_map == NULL) {
    NACL_NOTREACHED();
    return;
  }
  channel_to_instance_id_map->erase(channel);
  if (channel_to_instance_id_map->size() == 0) {
    delete channel_to_module_id_map;
    channel_to_module_id_map = NULL;
  }
}

PP_Module LookupModuleIdForSrpcChannel(NaClSrpcChannel* channel) {
  if (channel_to_module_id_map == NULL) {
    return 0;
  }
  return (*channel_to_module_id_map)[channel];
}

PP_Module LookupInstanceIdForSrpcChannel(NaClSrpcChannel* channel) {
  if (channel_to_instance_id_map == NULL) {
    return 0;
  }
  return (*channel_to_instance_id_map)[channel];
}

NaClSrpcChannel* GetMainSrpcChannel(NaClSrpcRpc* upcall_rpc) {
  // The upcall channel's server_instance_data member is initialized to point
  // to the main channel for this instance.  Here it is retrieved to use in
  // constructing a RemoteCallbackInfo.
  return static_cast<NaClSrpcChannel*>(
      upcall_rpc->channel->server_instance_data);
}

NaClSrpcChannel* GetMainSrpcChannel(PP_Instance instance) {
  return LookupBrowserPppForInstance(instance)->main_channel();
}

void CleanUpAfterDeadNexe(PP_Instance instance) {
  DebugPrintf("CleanUpAfterDeadNexe\n");
  BrowserPpp* proxy = LookupBrowserPppForInstance(instance);
  if (proxy == NULL)
    return;
  proxy->plugin()->ReportDeadNexe();  // Shuts down and deletes the proxy.
}

void SetPPBGetInterface(PPB_GetInterface get_interface_function,
                        bool allow_dev_interfaces,
                        bool allow_3d_interfaces) {
  get_interface = get_interface_function;
  enable_dev_interfaces = allow_dev_interfaces;
  enable_3d_interfaces = allow_3d_interfaces;
}

const void* GetBrowserInterface(const char* interface_name) {
  // Reject suspiciously long interface strings.
  const size_t kMaxLength = 1024;
  if (NULL == memchr(interface_name, '\0', kMaxLength)) {
    return NULL;
  }
  // If dev interface is not enabled, reject interfaces containing "(Dev)"
  if (!enable_dev_interfaces && strstr(interface_name, "(Dev)") != NULL) {
    return NULL;
  }
  if (!enable_3d_interfaces) {
    static const char* disabled_interface_names[] = {
      PPB_GRAPHICS_3D_INTERFACE,
      PPB_GRAPHICS_3D_TRUSTED_INTERFACE,
      PPB_GLES_CHROMIUM_TEXTURE_MAPPING_DEV_INTERFACE,
      PPB_OPENGLES2_INTERFACE,
      PPB_LAYER_COMPOSITOR_DEV_INTERFACE
    };
    for (size_t i = 0; i < NACL_ARRAY_SIZE(disabled_interface_names); i++) {
      if (strcmp(interface_name, disabled_interface_names[i]) == 0)
        return NULL;
    }
  }
  return (*get_interface)(interface_name);
}

const void* GetBrowserInterfaceSafe(const char* interface_name) {
  const void* ppb_interface = GetBrowserInterface(interface_name);
  if (ppb_interface == NULL)
    DebugPrintf("PPB_GetInterface: %s not found\n", interface_name);
  CHECK(ppb_interface != NULL);
  return ppb_interface;
}

const PPB_Core* PPBCoreInterface() {
  static const PPB_Core* ppb = static_cast<const PPB_Core*>(
      GetBrowserInterfaceSafe(PPB_CORE_INTERFACE));
  return ppb;
}

const PPB_Graphics2D* PPBGraphics2DInterface() {
  static const PPB_Graphics2D* ppb = static_cast<const PPB_Graphics2D*>(
      GetBrowserInterfaceSafe(PPB_GRAPHICS_2D_INTERFACE));
  return ppb;
}

const PPB_Graphics3D* PPBGraphics3DInterface() {
  static const PPB_Graphics3D* ppb = static_cast<const PPB_Graphics3D*>(
      GetBrowserInterfaceSafe(PPB_GRAPHICS_3D_INTERFACE));
  return ppb;
}

const PPB_Graphics3DTrusted* PPBGraphics3DTrustedInterface() {
  static const PPB_Graphics3DTrusted* ppb =
      static_cast<const PPB_Graphics3DTrusted*>(
          GetBrowserInterfaceSafe(PPB_GRAPHICS_3D_TRUSTED_INTERFACE));
  return ppb;
}

const PPB_ImageData* PPBImageDataInterface() {
  static const PPB_ImageData* ppb = static_cast<const PPB_ImageData*>(
      GetBrowserInterfaceSafe(PPB_IMAGEDATA_INTERFACE));
  return ppb;
}

const PPB_ImageDataTrusted* PPBImageDataTrustedInterface() {
  static const PPB_ImageDataTrusted* ppb =
      static_cast<const PPB_ImageDataTrusted*>(
      GetBrowserInterfaceSafe(PPB_IMAGEDATA_TRUSTED_INTERFACE));
  return ppb;
}

const PPB_InputEvent* PPBInputEventInterface() {
  static const PPB_InputEvent* ppb = static_cast<const PPB_InputEvent*>(
      GetBrowserInterfaceSafe(PPB_INPUT_EVENT_INTERFACE));
  return ppb;
}

const PPB_Instance* PPBInstanceInterface() {
  static const PPB_Instance* ppb = static_cast<const PPB_Instance*>(
      GetBrowserInterfaceSafe(PPB_INSTANCE_INTERFACE));
  return ppb;
}

const PPB_KeyboardInputEvent* PPBKeyboardInputEventInterface() {
  static const PPB_KeyboardInputEvent* ppb =
      static_cast<const PPB_KeyboardInputEvent*>(
          GetBrowserInterfaceSafe(PPB_KEYBOARD_INPUT_EVENT_INTERFACE));
  return ppb;
}

const PPB_Memory_Dev* PPBMemoryInterface() {
  static const PPB_Memory_Dev* ppb = static_cast<const PPB_Memory_Dev*>(
      GetBrowserInterfaceSafe(PPB_MEMORY_DEV_INTERFACE));
  return ppb;
}

const PPB_Messaging* PPBMessagingInterface() {
  static const PPB_Messaging* ppb =
      static_cast<const PPB_Messaging*>(
          GetBrowserInterfaceSafe(PPB_MESSAGING_INTERFACE));
  return ppb;
}

const PPB_MouseInputEvent* PPBMouseInputEventInterface() {
  static const PPB_MouseInputEvent* ppb =
      static_cast<const PPB_MouseInputEvent*>(
          GetBrowserInterfaceSafe(PPB_MOUSE_INPUT_EVENT_INTERFACE));
  return ppb;
}

const PPB_URLLoader* PPBURLLoaderInterface() {
  static const PPB_URLLoader* ppb =
      static_cast<const PPB_URLLoader*>(
          GetBrowserInterfaceSafe(PPB_URLLOADER_INTERFACE));
  return ppb;
}

const PPB_URLRequestInfo* PPBURLRequestInfoInterface() {
  static const PPB_URLRequestInfo* ppb =
      static_cast<const PPB_URLRequestInfo*>(
          GetBrowserInterfaceSafe(PPB_URLREQUESTINFO_INTERFACE));
  return ppb;
}

const PPB_URLResponseInfo* PPBURLResponseInfoInterface() {
  static const PPB_URLResponseInfo* ppb =
      static_cast<const PPB_URLResponseInfo*>(
          GetBrowserInterfaceSafe(PPB_URLRESPONSEINFO_INTERFACE));
  return ppb;
}

const PPB_Var* PPBVarInterface() {
  static const PPB_Var* ppb =
      static_cast<const PPB_Var*>(
          GetBrowserInterfaceSafe(PPB_VAR_INTERFACE));
  return ppb;
}

const PPB_WheelInputEvent* PPBWheelInputEventInterface() {
  static const PPB_WheelInputEvent* ppb =
      static_cast<const PPB_WheelInputEvent*>(
          GetBrowserInterfaceSafe(PPB_WHEEL_INPUT_EVENT_INTERFACE));
  return ppb;
}

// Dev interfaces.
const PPB_CursorControl_Dev* PPBCursorControlInterface() {
  static const PPB_CursorControl_Dev* ppb =
      static_cast<const PPB_CursorControl_Dev*>(
          GetBrowserInterfaceSafe(PPB_CURSOR_CONTROL_DEV_INTERFACE));
  return ppb;
}

const PPB_FileIO* PPBFileIOInterface() {
  static const PPB_FileIO* ppb =
      static_cast<const PPB_FileIO*>(
        GetBrowserInterfaceSafe(PPB_FILEIO_INTERFACE));
  return ppb;
}

const PPB_FileRef* PPBFileRefInterface() {
  static const PPB_FileRef* ppb =
      static_cast<const PPB_FileRef*>(
        GetBrowserInterfaceSafe(PPB_FILEREF_INTERFACE));
  return ppb;
}

const PPB_FileSystem* PPBFileSystemInterface() {
  static const PPB_FileSystem* ppb =
      static_cast<const PPB_FileSystem*>(
        GetBrowserInterfaceSafe(PPB_FILESYSTEM_INTERFACE));
  return ppb;
}

const PPB_Find_Dev* PPBFindInterface() {
  static const PPB_Find_Dev* ppb =
      static_cast<const PPB_Find_Dev*>(
        GetBrowserInterfaceSafe(PPB_FIND_DEV_INTERFACE));
  return ppb;
}

const PPB_Font_Dev* PPBFontInterface() {
  static const PPB_Font_Dev* ppb =
      static_cast<const PPB_Font_Dev*>(
        GetBrowserInterfaceSafe(PPB_FONT_DEV_INTERFACE));
  return ppb;
}

const PPB_Fullscreen* PPBFullscreenInterface() {
  static const PPB_Fullscreen* ppb =
      static_cast<const PPB_Fullscreen*>(
        GetBrowserInterfaceSafe(PPB_FULLSCREEN_INTERFACE));
  return ppb;
}

const PPB_MouseLock* PPBMouseLockInterface() {
  static const PPB_MouseLock* ppb = static_cast<const PPB_MouseLock*>(
      GetBrowserInterfaceSafe(PPB_MOUSELOCK_INTERFACE));
  return ppb;
}

const PPB_Scrollbar_Dev* PPBScrollbarInterface() {
  static const PPB_Scrollbar_Dev* ppb =
      static_cast<const PPB_Scrollbar_Dev*>(
          GetBrowserInterfaceSafe(PPB_SCROLLBAR_DEV_INTERFACE));
  return ppb;
}

const PPB_Testing_Dev* PPBTestingInterface() {
  static const PPB_Testing_Dev* ppb =
      static_cast<const PPB_Testing_Dev*>(
          GetBrowserInterfaceSafe(PPB_TESTING_DEV_INTERFACE));
  return ppb;
}

const PPB_Widget_Dev* PPBWidgetInterface() {
  static const PPB_Widget_Dev* ppb =
      static_cast<const PPB_Widget_Dev*>(
          GetBrowserInterfaceSafe(PPB_WIDGET_DEV_INTERFACE));
  return ppb;
}

const PPB_Zoom_Dev* PPBZoomInterface() {
  static const PPB_Zoom_Dev* ppb =
      static_cast<const PPB_Zoom_Dev*>(
          GetBrowserInterfaceSafe(PPB_ZOOM_DEV_INTERFACE));
  return ppb;
}

// Private interfaces.
const PPB_PDF* PPBPDFInterface() {
  static const PPB_PDF* ppb =
      static_cast<const PPB_PDF*>(
          GetBrowserInterfaceSafe(PPB_PDF_INTERFACE));
  return ppb;
}

const PPB_TCPSocket_Private* PPBTCPSocketPrivateInterface() {
  static const PPB_TCPSocket_Private* ppb =
      static_cast<const PPB_TCPSocket_Private*>(
          GetBrowserInterfaceSafe(PPB_TCPSOCKET_PRIVATE_INTERFACE));
  return ppb;
}

const PPB_UDPSocket_Private* PPBUDPSocketPrivateInterface() {
  static const PPB_UDPSocket_Private* ppb =
      static_cast<const PPB_UDPSocket_Private*>(
          GetBrowserInterfaceSafe(PPB_UDPSOCKET_PRIVATE_INTERFACE));
  return ppb;
}

}  // namespace ppapi_proxy
