/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <stdio.h>
#include <string.h>

#include <map>

#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/desc_based_handle.h"
#include "native_client/src/trusted/plugin/portable_handle.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"
#include "native_client/src/trusted/plugin/shared_memory.h"
#include "native_client/src/trusted/plugin/plugin.h"


namespace {

bool Map(void* obj, plugin::SrpcParams* params) {
  plugin::DescBasedHandle *ptr =
      reinterpret_cast<plugin::DescBasedHandle*>(obj);
  // Create a copy of the wrapper to go on the SharedMemory object.
  nacl::DescWrapper* shm_wrapper =
      ptr->plugin()->wrapper_factory()->MakeGeneric(ptr->wrapper()->desc());
  // Increment the ref count of the contained object.
  NaClDescRef(shm_wrapper->desc());

  plugin::SharedMemory* portable_shared_memory =
      plugin::SharedMemory::New(ptr->plugin(), shm_wrapper);
  plugin::ScriptableHandle* shared_memory =
    ptr->browser_interface()->NewScriptableHandle(portable_shared_memory);
  if (NULL == shared_memory) {
    return false;
  }
  PLUGIN_PRINTF(("Map (shared_memory=%p)\n",
                 static_cast<void*>(shared_memory)));
  params->outs()[0]->tag = NACL_SRPC_ARG_TYPE_OBJECT;
  params->outs()[0]->arrays.oval = shared_memory;
  return true;
}

}  // namespace

namespace plugin {

DescBasedHandle::DescBasedHandle()
    : plugin_(NULL), wrapper_(NULL) {
  PLUGIN_PRINTF(("DescBasedHandle::DescBasedHandle (this=%p)\n",
                 static_cast<void*>(this)));
}

DescBasedHandle::~DescBasedHandle() {
  PLUGIN_PRINTF(("DescBasedHandle::~DescBasedHandle (this=%p)\n",
                 static_cast<void*>(this)));
  if (NULL != wrapper_) {
    delete wrapper_;
    wrapper_ = NULL;
  }
}

DescBasedHandle* DescBasedHandle::New(Plugin* plugin,
                                      nacl::DescWrapper* wrapper) {
  PLUGIN_PRINTF(("DescBasedHandle::New (plugin=%p)\n",
                 static_cast<void*>(plugin)));

  DescBasedHandle* desc_based_handle = new(std::nothrow) DescBasedHandle();

  if (desc_based_handle == NULL || !desc_based_handle->Init(plugin, wrapper)) {
    return NULL;
  }

  return desc_based_handle;
}

bool DescBasedHandle::Init(Plugin* plugin, nacl::DescWrapper* wrapper) {
  plugin_ = plugin;
  wrapper_ = wrapper;
  LoadMethods();
  return true;
}

BrowserInterface* DescBasedHandle::browser_interface() const {
  return plugin_->browser_interface();
}

void DescBasedHandle::LoadMethods() {
  // Methods supported by DescBasedHandle.
  AddMethodCall(Map, "map", "", "h");
}

}  // namespace plugin
