// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "base/logging.h"
#include "o3d/gpu_plugin/np_utils/np_utils.h"
#include "o3d/gpu_plugin/gpu_plugin_object.h"
#include "o3d/gpu_plugin/gpu_processor.h"

namespace o3d {
namespace gpu_plugin {

const NPUTF8 GPUPluginObject::kPluginType[] =
    "application/vnd.google.chrome.gpu-plugin";

GPUPluginObject::GPUPluginObject(NPP npp)
    : npp_(npp),
      status_(CREATED) {
  memset(&window_, 0, sizeof(window_));
}

NPError GPUPluginObject::New(NPMIMEType plugin_type,
                             int16 argc,
                             char* argn[],
                             char* argv[],
                             NPSavedData* saved) {
  if (status_ != CREATED)
    return NPERR_GENERIC_ERROR;

  status_ = INITIALIZED;

  return NPERR_NO_ERROR;
}

NPError GPUPluginObject::SetWindow(NPWindow* new_window) {
  if (status_ != INITIALIZED)
    return NPERR_GENERIC_ERROR;

  NPError error = PlatformSpecificSetWindow(new_window);
  if (error == NPERR_NO_ERROR) {
    window_ = *new_window;
  } else {
    memset(&window_, 0, sizeof(window_));
  }

  UpdateProcessorWindow();

  return error;
}

int16 GPUPluginObject::HandleEvent(NPEvent* event) {
  return 0;
}

NPError GPUPluginObject::Destroy(NPSavedData** saved) {
  if (status_ != INITIALIZED)
    return NPERR_GENERIC_ERROR;

  processor_ = NULL;
  if (command_buffer_.Get()) {
    command_buffer_->SetPutOffsetChangeCallback(NULL);
    command_buffer_ = NPObjectPointer<CommandBuffer>();
  }

  status_ = DESTROYED;

  return NPERR_NO_ERROR;
}

void GPUPluginObject::Release() {
  DCHECK(status_ != INITIALIZED);
  NPBrowser::get()->ReleaseObject(this);
}

NPObject*GPUPluginObject::GetScriptableNPObject() {
  NPBrowser::get()->RetainObject(this);
  return this;
}

NPObjectPointer<NPObject> GPUPluginObject::OpenCommandBuffer() {
  if (command_buffer_.Get())
    return command_buffer_;

  NPObjectPointer<NPObject> window = NPObjectPointer<NPObject>::FromReturned(
      NPBrowser::get()->GetWindowNPObject(npp_));
  if (!window.Get())
    return NPObjectPointer<NPObject>();

  NPObjectPointer<NPObject> chromium;
  if (!NPGetProperty(npp_, window, "chromium", &chromium)) {
    return NPObjectPointer<NPObject>();
  }

  NPObjectPointer<NPObject> system;
  if (!NPGetProperty(npp_, chromium, "system", &system)) {
    return NPObjectPointer<NPObject>();
  }

  NPObjectPointer<NPObject> ring_buffer;
  if (!NPInvoke(npp_, system, "createSharedMemory", kCommandBufferSize,
                &ring_buffer)) {
    return NPObjectPointer<NPObject>();
  }

  if (!ring_buffer.Get()) {
    return NPObjectPointer<NPObject>();
  }

  command_buffer_ = NPCreateObject<CommandBuffer>(npp_);
  if (command_buffer_->Initialize(ring_buffer)) {
    processor_ = new GPUProcessor(npp_, command_buffer_.Get());
    if (processor_->Initialize(static_cast<HWND>(window_.window))) {
      command_buffer_->SetPutOffsetChangeCallback(
          NewCallback(processor_.get(),
                      &GPUProcessor::ProcessCommands));
      UpdateProcessorWindow();
      return command_buffer_;
    }
  }

  processor_ = NULL;
  command_buffer_ = NPObjectPointer<CommandBuffer>();
  return command_buffer_;
}

}  // namespace gpu_plugin
}  // namespace o3d
