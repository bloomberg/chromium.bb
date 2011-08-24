// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_proxy_impl.h"

#include "ppapi/c/private/ppb_proxy_private.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_image_data_api.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_url_loader_impl.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

using ppapi::thunk::EnterResource;
using ppapi::thunk::PPB_URLLoader_API;

namespace webkit {
namespace ppapi {

namespace {

void PluginCrashed(PP_Module module) {
  PluginModule* plugin_module = ResourceTracker::Get()->GetModule(module);
  if (plugin_module)
    plugin_module->PluginCrashed();
}

PP_Instance GetInstanceForResource(PP_Resource resource) {
  ::ppapi::Resource* obj = ResourceTracker::Get()->GetResource(resource);
  if (!obj)
    return 0;
  return obj->pp_instance();
}

void SetReserveInstanceIDCallback(PP_Module module,
                                  PP_Bool (*reserve)(PP_Module, PP_Instance)) {
  PluginModule* plugin_module = ResourceTracker::Get()->GetModule(module);
  if (plugin_module)
    plugin_module->SetReserveInstanceIDCallback(reserve);
}

int32_t GetURLLoaderBufferedBytes(PP_Resource url_loader) {
  EnterResource<PPB_URLLoader_API> enter(url_loader, true);
  if (enter.succeeded())
    return static_cast<PPB_URLLoader_Impl*>(enter.object())->buffer_size();
  return 0;
}

void AddRefModule(PP_Module module) {
  PluginModule* plugin_module = ResourceTracker::Get()->GetModule(module);
  if (plugin_module)
    plugin_module->AddRef();
}

void ReleaseModule(PP_Module module) {
  PluginModule* plugin_module = ResourceTracker::Get()->GetModule(module);
  if (plugin_module)
    plugin_module->Release();
}

const PPB_Proxy_Private ppb_proxy = {
  &PluginCrashed,
  &GetInstanceForResource,
  &SetReserveInstanceIDCallback,
  &GetURLLoaderBufferedBytes,
  &AddRefModule,
  &ReleaseModule
};

}  // namespace

// static
const PPB_Proxy_Private* PPB_Proxy_Impl::GetInterface() {
  return &ppb_proxy;
}

}  // namespace ppapi
}  // namespace webkit
