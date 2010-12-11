// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include <stdio.h>
#include <map>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"


namespace ppapi_proxy {

// All of these methods are called from the browser main (UI, JavaScript, ...)
// thread.

const PP_Resource kInvalidResourceId = 0;


namespace {

std::map<PP_Instance, BrowserPpp*>* instance_to_ppp_map = NULL;
std::map<NaClSrpcChannel*, PP_Module>* channel_to_module_id_map = NULL;

// The function pointer from the browser
// Set by SetPPBGetInterface().
PPB_GetInterface get_interface;

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

PP_Module LookupModuleIdForSrpcChannel(NaClSrpcChannel* channel) {
  if (channel_to_module_id_map == NULL) {
    return NULL;
  }
  return (*channel_to_module_id_map)[channel];
}

void SetPPBGetInterface(PPB_GetInterface get_interface_function) {
  get_interface = get_interface_function;
}

const void* GetBrowserInterface(const char* interface_name) {
  return (*get_interface)(interface_name);
}

const void* GetBrowserInterfaceSafe(const char* interface_name) {
  const void* ppb_interface = (*get_interface)(interface_name);
  if (ppb_interface == NULL)
    DebugPrintf("Browser::PPB_GetInterface: %s not found\n", interface_name);
  CHECK(ppb_interface != NULL);
  return ppb_interface;
}

const PPB_Core* PPBCoreInterface() {
  static const PPB_Core* core_interface = NULL;
  if (core_interface == NULL) {
    core_interface = reinterpret_cast<const PPB_Core*>(
        GetBrowserInterfaceSafe(PPB_CORE_INTERFACE));
  }
  return core_interface;
}

const PPB_Graphics2D* PPBGraphics2DInterface() {
  static const PPB_Graphics2D* graphics2d_interface = NULL;
  if (graphics2d_interface == NULL) {
    graphics2d_interface = reinterpret_cast<const PPB_Graphics2D*>(
        GetBrowserInterfaceSafe(PPB_GRAPHICS_2D_INTERFACE));
  }
  return graphics2d_interface;
}

const PPB_ImageData* PPBImageDataInterface() {
  static const PPB_ImageData* image_data_interface = NULL;
  if (image_data_interface == NULL) {
    image_data_interface = reinterpret_cast<const PPB_ImageData*>(
        GetBrowserInterfaceSafe(PPB_IMAGEDATA_INTERFACE));
  }
  return image_data_interface;
}

const PPB_Var_Deprecated* PPBVarInterface() {
  static const PPB_Var_Deprecated* var_interface = NULL;
  if (var_interface == NULL) {
    var_interface = reinterpret_cast<const PPB_Var_Deprecated*>(
        GetBrowserInterfaceSafe(PPB_VAR_DEPRECATED_INTERFACE));
  }
  return var_interface;
}

}  // namespace ppapi_proxy
