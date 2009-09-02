// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/np_utils/dispatched_np_object.h"

namespace o3d {
namespace gpu_plugin {

DispatchedNPObject::DispatchedNPObject(NPP npp)
    : BaseNPObject(npp) {
}

bool DispatchedNPObject::HasMethod(NPIdentifier name) {
  for (BaseNPDispatcher* dispatcher = GetDynamicDispatcherChain();
       dispatcher;
       dispatcher = dispatcher->next()) {
    if (dispatcher->name() == name) {
      return true;
    }
  }

  return false;
}

bool DispatchedNPObject::Invoke(NPIdentifier name,
                                const NPVariant* args,
                                uint32_t num_args,
                                NPVariant* result) {
  VOID_TO_NPVARIANT(*result);

  for (BaseNPDispatcher* dispatcher = GetDynamicDispatcherChain();
       dispatcher;
       dispatcher = dispatcher->next()) {
    if (dispatcher->name() == name && dispatcher->num_args() == num_args) {
      if (dispatcher->Invoke(this, args, num_args, result))
        return true;
    }
  }

  return false;
}

bool DispatchedNPObject::Enumerate(NPIdentifier** names, uint32_t* num_names) {
  // Count the number of names.
  *num_names = 0;
  for (BaseNPDispatcher* dispatcher = GetDynamicDispatcherChain();
       dispatcher;
       dispatcher = dispatcher->next()) {
    ++(*num_names);
  }

  // Copy names into the array.
  *names = static_cast<NPIdentifier*>(
      NPBrowser::get()->MemAlloc((*num_names) * sizeof(**names)));
  int i = 0;
  for (BaseNPDispatcher* dispatcher = GetDynamicDispatcherChain();
       dispatcher;
       dispatcher = dispatcher->next()) {
    (*names)[i] = dispatcher->name();
    ++i;
  }

  return true;
}

BaseNPDispatcher* DispatchedNPObject::GetStaticDispatcherChain() {
  return NULL;
}

BaseNPDispatcher* DispatchedNPObject::GetDynamicDispatcherChain() {
  return NULL;
}

}  // namespace gpu_plugin
}  // namespace o3d
