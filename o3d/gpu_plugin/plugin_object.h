// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_PLUGIN_OBJECT_H_
#define O3D_GPU_PLUGIN_PLUGIN_OBJECT_H_

#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"

namespace o3d {
namespace gpu_plugin {

// Interface for a plugin instance. The NPP plugin calls forwards to an instance
// of this interface.
class PluginObject {
 public:
  PluginObject() {
  }

  virtual ~PluginObject() {
  }

  virtual NPError New(NPMIMEType plugin_type,
                      int16 argc,
                      char* argn[],
                      char* argv[],
                      NPSavedData* saved) = 0;

  virtual NPError SetWindow(NPWindow* new_window) = 0;

  virtual int16 HandleEvent(NPEvent* event) = 0;

  virtual NPError Destroy(NPSavedData** saved) = 0;

  virtual NPObject* GetScriptableInstance() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PluginObject);
};

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_PLUGIN_OBJECT_H_
