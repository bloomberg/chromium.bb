// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_GPU_PLUGIN_OBJECT_H_
#define O3D_GPU_PLUGIN_GPU_PLUGIN_OBJECT_H_

#include <string>

#include "base/ref_counted.h"
#include "base/thread.h"
#include "o3d/gpu_plugin/command_buffer.h"
#include "o3d/gpu_plugin/gpu_processor.h"
#include "o3d/gpu_plugin/np_utils/default_np_object.h"
#include "o3d/gpu_plugin/np_utils/np_dispatcher.h"
#include "o3d/gpu_plugin/np_utils/np_headers.h"
#include "o3d/gpu_plugin/np_utils/np_plugin_object.h"
#include "o3d/gpu_plugin/np_utils/np_utils.h"

namespace gpu_plugin {

// The scriptable object for the GPU plugin.
class GPUPluginObject : public DefaultNPObject<NPObject>,
                        public PluginObject {
 public:
  static const int32 kCommandBufferSize = 1024 * 1024;

  enum Status {
    // In the state of waiting for the named function to be called to continue
    // the initialization sequence.
    kWaitingForNew,
    kWaitingForSetWindow,
    kWaitingForOpenCommandBuffer,

    // Initialization either succeeded or failed.
    kInitializationSuccessful,
    kInitializationFailed,

    // Destroy has now been called and the plugin object cannot be used.
    kDestroyed,
  };

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

  // Returns the current initialization status. See Status enum.
  int32 GetStatus() {
    return status_;
  }

  // Get the width of the plugin window.
  int32 GetWidth() {
    return window_.width;
  }

  // Get the height of the plugin window.
  int32 GetHeight() {
    return window_.height;
  }

  // Set the object that receives notifications of GPU plugin object events
  // such as resize and keyboard and mouse input.
  void SetEventSync(NPObjectPointer<NPObject> event_sync) {
    event_sync_ = event_sync;
  }

  NPObjectPointer<NPObject> GetEventSync() {
    return event_sync_;
  }

  // Initializes and returns the command buffer object. Returns NULL if the
  // command buffer cannot be initialized, for example if the plugin does not
  // yet have a window handle.
  NPObjectPointer<NPObject> OpenCommandBuffer();

  // Set the status for testing.
  void set_status(Status status) {
    status_ = status;
  }

  // Replace the default command buffer for testing.
  void set_command_buffer(
      const NPObjectPointer<CommandBuffer>& command_buffer) {
    command_buffer_ = command_buffer;
  }

  // Replace the default GPU processor for testing.
  void set_gpu_processor(const scoped_refptr<GPUProcessor>& processor) {
    processor_ = processor;
  }

  NP_UTILS_BEGIN_DISPATCHER_CHAIN(GPUPluginObject, DefaultNPObject<NPObject>)
    NP_UTILS_DISPATCHER(GetStatus, int32());
    NP_UTILS_DISPATCHER(GetWidth, int32());
    NP_UTILS_DISPATCHER(GetHeight, int32());
    NP_UTILS_DISPATCHER(SetEventSync, void(NPObjectPointer<NPObject> sync));
    NP_UTILS_DISPATCHER(GetEventSync, NPObjectPointer<NPObject>());
    NP_UTILS_DISPATCHER(OpenCommandBuffer, NPObjectPointer<NPObject>())
  NP_UTILS_END_DISPATCHER_CHAIN

 private:
  NPError PlatformSpecificSetWindow(NPWindow* new_window);

  NPP npp_;
  Status status_;
  NPWindow window_;
  NPObjectPointer<CommandBuffer> command_buffer_;
  scoped_refptr<GPUProcessor> processor_;
  NPObjectPointer<NPObject> event_sync_;
};

}  // namespace gpu_plugin

#endif  // O3D_GPU_PLUGIN_GPU_PLUGIN_OBJECT_H_
