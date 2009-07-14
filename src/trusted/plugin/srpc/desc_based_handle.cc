/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */




#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include <map>
#include <string>

#include "native_client/src/include/base/basictypes.h"
#include "native_client/src/trusted/plugin/srpc/desc_based_handle.h"
#include "native_client/src/trusted/plugin/srpc/portable_handle.h"
#include "native_client/src/trusted/plugin/srpc/shared_memory.h"

#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"

namespace nacl_srpc {

  int DescBasedHandle::number_alive_counter = 0;

  DescBasedHandle::DescBasedHandle(): plugin_(NULL),
                                      desc_(NULL) {
    dprintf(("DescBasedHandle::DescBasedHandle(%p, %d)\n",
             static_cast<void *>(this),
             ++number_alive_counter));
  }

  DescBasedHandle::~DescBasedHandle() {
    dprintf(("DescBasedHandle::~DescBasedHandle(%p, %d)\n",
             static_cast<void *>(this),
             --number_alive_counter));
    if (NULL != desc_) {
      NaClDescUnref(desc_);
    }
  }

  bool DescBasedHandle::Init(struct PortableHandleInitializer* init_info) {
    if (!PortableHandle::Init(init_info)) {
      return false;
    }
    DescHandleInitializer *desc_init_info =
        static_cast<DescHandleInitializer*>(init_info);
    desc_ = desc_init_info->desc_;
    plugin_ = desc_init_info->plugin_;
    NaClDescRef(desc_);
    LoadMethods();
    return true;
  }

  void DescBasedHandle::LoadMethods() {
    // the only method supported by PortableHandle
    AddMethodToMap(Map, "map", METHOD_CALL, "", "h");
  }

  bool DescBasedHandle::Map(void* obj, SrpcParams* params) {
    DescBasedHandle *ptr = reinterpret_cast<DescBasedHandle*>(obj);
    struct SharedMemoryInitializer init_info(ptr->GetPortablePluginInterface(),
        ptr->desc_, ptr->plugin_);

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
