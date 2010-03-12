/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include <map>

#include "native_client/src/trusted/plugin/srpc/desc_based_handle.h"
#include "native_client/src/trusted/plugin/srpc/portable_handle.h"
#include "native_client/src/trusted/plugin/srpc/shared_memory.h"

#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"

namespace nacl_srpc {

  int DescBasedHandle::number_alive_counter = 0;

  DescBasedHandle::DescBasedHandle(): plugin_(NULL),
                                      wrapper_(NULL) {
    dprintf(("DescBasedHandle::DescBasedHandle(%p, %d)\n",
             static_cast<void *>(this),
             ++number_alive_counter));
  }

  DescBasedHandle::~DescBasedHandle() {
    dprintf(("DescBasedHandle::~DescBasedHandle(%p, %d)\n",
             static_cast<void *>(this),
             --number_alive_counter));
    if (NULL != wrapper_) {
      wrapper_->Delete();
      wrapper_ = NULL;
    }
  }

  bool DescBasedHandle::Init(struct PortableHandleInitializer* init_info) {
    if (!PortableHandle::Init(init_info)) {
      return false;
    }
    DescHandleInitializer *desc_init_info =
        static_cast<DescHandleInitializer*>(init_info);
    wrapper_ = desc_init_info->wrapper_;
    plugin_ = desc_init_info->plugin_;
    LoadMethods();
    return true;
  }

  void DescBasedHandle::LoadMethods() {
    // the only method supported by PortableHandle
    AddMethodToMap(Map, "map", METHOD_CALL, "", "h");
  }

  bool DescBasedHandle::Map(void* obj, SrpcParams* params) {
    DescBasedHandle *ptr = reinterpret_cast<DescBasedHandle*>(obj);
    // Create a copy of the wrapper to go on the SharedMemory object.
    nacl::DescWrapper* shm_wrapper =
        ptr->plugin_->wrapper_factory()->MakeGeneric(ptr->wrapper_->desc());
    // Increment the ref count of the contained object.
    NaClDescRef(shm_wrapper->desc());
    struct SharedMemoryInitializer init_info(ptr->GetPortablePluginInterface(),
        shm_wrapper, ptr->plugin_);

    ScriptableHandle<SharedMemory>* shared_memory =
      ScriptableHandle<SharedMemory>::New(
          static_cast<PortableHandleInitializer*>(&init_info));
    if (NULL == shared_memory) {
      return false;
    }
    dprintf(("ScriptableHandle::Invoke: new returned %p\n",
             static_cast<void *>(shared_memory)));
    params->outs[0]->tag = NACL_SRPC_ARG_TYPE_OBJECT;
    params->outs[0]->u.oval =
        static_cast<BrowserScriptableObject*>(shared_memory);
    return true;
  }

  Plugin* DescBasedHandle::GetPlugin() {
    return plugin_;
  }
}
