// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_NP_UTILS_NP_PLUGIN_OBJECT_FACTORY_H_
#define O3D_GPU_PLUGIN_NP_UTILS_NP_PLUGIN_OBJECT_FACTORY_H_

#include "base/basictypes.h"
#include "o3d/gpu_plugin/np_utils/np_headers.h"

namespace gpu_plugin {

class PluginObject;

// Mockable factory base class used to create instances of PluginObject based on
// plugin mime type.
class NPPluginObjectFactory {
 public:
  virtual PluginObject* CreatePluginObject(NPP npp, NPMIMEType plugin_type);

  static NPPluginObjectFactory* get() {
    return factory_;
  }

 protected:
  NPPluginObjectFactory();
  virtual ~NPPluginObjectFactory();

 private:
  static NPPluginObjectFactory* factory_;
  NPPluginObjectFactory* previous_factory_;
  DISALLOW_COPY_AND_ASSIGN(NPPluginObjectFactory);
};

}  // namespace gpu_plugin

#endif  // O3D_GPU_PLUGIN_NP_UTILS_NP_PLUGIN_OBJECT_FACTORY_H_
