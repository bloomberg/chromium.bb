// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_GPU_PLUGIN_OBJECT_H_
#define O3D_GPU_PLUGIN_GPU_PLUGIN_OBJECT_H_

#include <string>

#include "o3d/gpu_plugin/np_utils/dispatched_np_object.h"
#include "o3d/gpu_plugin/np_utils/np_dispatcher.h"
#include "o3d/gpu_plugin/np_utils/np_plugin_object.h"
#include "o3d/gpu_plugin/np_utils/np_utils.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"

struct NaClDesc;

namespace o3d {
namespace gpu_plugin {

// The scriptable object for the GPU plugin.
class GPUPluginObject : public DispatchedNPObject, public PluginObject {
 public:
  static const NPUTF8 kPluginType[];

  explicit GPUPluginObject(NPP npp);

  virtual NPError New(NPMIMEType plugin_type,
                      int16 argc,
                      char* argn[],
                      char* argv[],
                      NPSavedData* saved);

  virtual NPError SetWindow(NPWindow* new_window);
  const NPWindow& GetWindow() { return window_; }

  virtual int16 HandleEvent(NPEvent* event);

  virtual NPError Destroy(NPSavedData** saved);

  virtual void Release();

  virtual NPObject* GetScriptableNPObject();

  // Opens and returns the command buffer shared memory object.
  NPObjectPointer<NPObject> OpenCommandBuffer();

 protected:
  NP_UTILS_BEGIN_DISPATCHER_CHAIN(GPUPluginObject, DispatchedNPObject)
    NP_UTILS_DISPATCHER(OpenCommandBuffer, NPObjectPointer<NPObject>())
  NP_UTILS_END_DISPATCHER_CHAIN

 private:
  NPError PlatformSpecificSetWindow(NPWindow* new_window);

  enum Status {
    CREATED,
    INITIALIZED,
    DESTROYED,
  };

  Status status_;
  NPWindow window_;
  NPObjectPointer<NPObject> command_buffer_object_;
  NPSharedMemory* shared_memory_;
};

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_GPU_PLUGIN_OBJECT_H_
