// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_NP_UTILS_NP_PLUGIN_OBJECT_H_
#define O3D_GPU_PLUGIN_NP_UTILS_NP_PLUGIN_OBJECT_H_

#include "o3d/gpu_plugin/np_utils/np_object_pointer.h"
#include "o3d/gpu_plugin/np_utils/np_headers.h"

namespace gpu_plugin {

// Interface for a plugin instance. The NPP plugin calls forwards to an instance
// of this interface.
class PluginObject {
 public:
  // Initialize this object.
  virtual NPError New(NPMIMEType plugin_type,
                      int16 argc,
                      char* argn[],
                      char* argv[],
                      NPSavedData* saved) = 0;

  virtual NPError SetWindow(NPWindow* new_window) = 0;

  virtual int16 HandleEvent(NPEvent* event) = 0;

  // Uninitialize but do not deallocate the object. Release will be called to
  // deallocate if Destroy succeeds.
  virtual NPError Destroy(NPSavedData** saved) = 0;

  // Deallocate this object. This object is invalid after this returns.
  virtual void Release() = 0;

  virtual NPObject* GetScriptableNPObject() = 0;

 protected:
  PluginObject() {
  }

  virtual ~PluginObject() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PluginObject);
};

}  // namespace gpu_plugin

#endif  // O3D_GPU_PLUGIN_NP_UTILS_NP_PLUGIN_OBJECT_H_
