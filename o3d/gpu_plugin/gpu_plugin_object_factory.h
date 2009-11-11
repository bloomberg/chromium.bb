// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_GPU_PLUGIN_OBJECT_FACTORY_H_
#define O3D_GPU_PLUGIN_GPU_PLUGIN_OBJECT_FACTORY_H_

#include "o3d/gpu_plugin/np_utils/np_plugin_object_factory.h"

namespace gpu_plugin {

// Plugin object factory for creating the GPUPluginObject.
class GPUPluginObjectFactory : public NPPluginObjectFactory {
 public:
  GPUPluginObjectFactory();
  virtual ~GPUPluginObjectFactory();

  virtual PluginObject* CreatePluginObject(NPP npp, NPMIMEType plugin_type);

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUPluginObjectFactory);
};

}  // namespace gpu_plugin

#endif  // O3D_GPU_PLUGIN_GPU_PLUGIN_OBJECT_FACTORY_H_
