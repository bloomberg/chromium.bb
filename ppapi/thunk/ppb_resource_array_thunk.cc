// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_resource_array_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance,
                   const PP_Resource elements[],
                   uint32_t size) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateResourceArray(instance, elements, size);
}

PP_Bool IsResourceArray(PP_Resource resource) {
  EnterResource<PPB_ResourceArray_API> enter(resource, false);
  return enter.succeeded() ? PP_TRUE : PP_FALSE;
}

uint32_t GetSize(PP_Resource resource_array) {
  EnterResource<PPB_ResourceArray_API> enter(resource_array, true);
  return enter.succeeded() ? enter.object()->GetSize() : 0;
}

PP_Resource GetAt(PP_Resource resource_array, uint32_t index) {
  EnterResource<PPB_ResourceArray_API> enter(resource_array, true);
  return enter.succeeded() ? enter.object()->GetAt(index) : 0;
}

const PPB_ResourceArray_Dev g_ppb_resource_array_thunk = {
  &Create,
  &IsResourceArray,
  &GetSize,
  &GetAt
};

}  // namespace

const PPB_ResourceArray_Dev_0_1* GetPPB_ResourceArray_Dev_0_1_Thunk() {
  return &g_ppb_resource_array_thunk;
}

}  // namespace thunk
}  // namespace ppapi
