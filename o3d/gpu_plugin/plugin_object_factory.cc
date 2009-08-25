// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/gpu_plugin_object.h"
#include "o3d/gpu_plugin/plugin_object_factory.h"

namespace o3d {
namespace gpu_plugin {

PluginObjectFactory::PluginObjectFactory() {
}

PluginObjectFactory::~PluginObjectFactory() {
}

PluginObject* PluginObjectFactory::CreatePluginObject(NPP npp,
                                                      NPMIMEType plugin_type) {
  if (strcmp(plugin_type, GPUPluginObject::kPluginType) == 0) {
    NPClass* np_class = const_cast<NPClass*>(
        BaseNPObject::GetNPClass<GPUPluginObject>());
    return static_cast<GPUPluginObject*>(
        gpu_plugin::NPN_CreateObject(npp, np_class));
  }

  return NULL;
}

}  // namespace gpu_plugin
}  // namespace o3d
