// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_GPU_PLUGIN_OBJECT_H_
#define O3D_GPU_PLUGIN_GPU_PLUGIN_OBJECT_H_

#include <string>

#include "base/ref_counted.h"
#include "base/thread.h"
#include "o3d/gpu_plugin/command_buffer.h"
#include "o3d/gpu_plugin/np_utils/default_np_object.h"
#include "o3d/gpu_plugin/np_utils/np_dispatcher.h"
#include "o3d/gpu_plugin/np_utils/np_headers.h"
#include "o3d/gpu_plugin/np_utils/np_plugin_object.h"
#include "o3d/gpu_plugin/np_utils/np_utils.h"

namespace o3d {
namespace gpu_plugin {

class GPUProcessor;

// The scriptable object for the GPU plugin.
class GPUPluginObject : public DefaultNPObject<NPObject>,
                        public PluginObject {
 public:
  static const int32 kCommandBufferSize = 1024 * 1024;

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

  // Initializes and returns the command buffer object.
  NPObjectPointer<NPObject> OpenCommandBuffer();

  NP_UTILS_BEGIN_DISPATCHER_CHAIN(GPUPluginObject, DefaultNPObject<NPObject>)
    NP_UTILS_DISPATCHER(OpenCommandBuffer, NPObjectPointer<NPObject>())
  NP_UTILS_END_DISPATCHER_CHAIN

 private:
  NPError PlatformSpecificSetWindow(NPWindow* new_window);
  void UpdateProcessorWindow();

  enum Status {
    CREATED,
    INITIALIZED,
    DESTROYED,
  };

  NPP npp_;
  Status status_;
  NPWindow window_;
  NPObjectPointer<CommandBuffer> command_buffer_;
  scoped_refptr<GPUProcessor> processor_;
};

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_GPU_PLUGIN_OBJECT_H_
