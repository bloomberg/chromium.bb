// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "base/logging.h"
#include "o3d/gpu_plugin/gpu_plugin_object.h"

namespace o3d {
namespace gpu_plugin {

const NPUTF8 GPUPluginObject::kPluginType[] =
    "application/vnd.google.chrome.gpu-plugin";

GPUPluginObject::GPUPluginObject(NPP npp)
    : DispatchedNPObject(npp),
      status_(CREATED),
      shared_memory_(NULL) {
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

  return error;
}

int16 GPUPluginObject::HandleEvent(NPEvent* event) {
  return 0;
}

NPError GPUPluginObject::Destroy(NPSavedData** saved) {
  if (status_ != INITIALIZED)
    return NPERR_GENERIC_ERROR;

  if (shared_memory_) {
    NPBrowser::get()->UnmapSharedMemory(npp(), shared_memory_);
  }

  command_buffer_object_ = NPObjectPointer<NPObject>();

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
  if (command_buffer_object_.Get())
    return command_buffer_object_;

  NPObjectPointer<NPObject> window = NPObjectPointer<NPObject>::FromReturned(
      NPBrowser::get()->GetWindowNPObject(npp()));
  if (!window.Get())
    return NPObjectPointer<NPObject>();

  NPObjectPointer<NPObject> chromium;
  if (!NPGetProperty(npp(), window, "chromium", &chromium)) {
    return NPObjectPointer<NPObject>();
  }

  NPObjectPointer<NPObject> system;
  if (!NPGetProperty(npp(), chromium, "system", &system)) {
    return NPObjectPointer<NPObject>();
  }

  if (!NPInvoke(npp(), system, "createSharedMemory", 1024,
                &command_buffer_object_)) {
    return NPObjectPointer<NPObject>();
  }

  shared_memory_ = NPBrowser::get()->MapSharedMemory(
      npp(), command_buffer_object_.Get(), 1024, false);
  if (!shared_memory_) {
    command_buffer_object_ = NPObjectPointer<NPObject>();
    return NPObjectPointer<NPObject>();
  }

  return command_buffer_object_;
}

}  // namespace gpu_plugin
}  // namespace o3d
