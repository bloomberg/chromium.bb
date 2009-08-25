// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_PLUGIN_OBJECT_FACTORY_H_
#define O3D_GPU_PLUGIN_PLUGIN_OBJECT_FACTORY_H_

#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"

namespace o3d {
namespace gpu_plugin {

class PluginObject;

// Mockable factory used to create instances of PluginObject based on plugin
// mime type.
class PluginObjectFactory {
 public:
  PluginObjectFactory();
  virtual ~PluginObjectFactory();

  virtual PluginObject* CreatePluginObject(NPP npp, NPMIMEType plugin_type);

 private:
  DISALLOW_COPY_AND_ASSIGN(PluginObjectFactory);
};

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_PLUGIN_OBJECT_FACTORY_H_
