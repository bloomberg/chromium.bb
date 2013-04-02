// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From dev/ppb_resource_array_dev.idl modified Thu Dec 20 13:10:26 2012.

#include "ppapi/c/dev/ppb_resource_array_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/ppb_resource_array_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance,
                   const PP_Resource elements[],
                   uint32_t size) {
  VLOG(4) << "PPB_ResourceArray_Dev::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateResourceArray(instance, elements, size);
}

PP_Bool IsResourceArray(PP_Resource resource) {
  VLOG(4) << "PPB_ResourceArray_Dev::IsResourceArray()";
  EnterResource<PPB_ResourceArray_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

uint32_t GetSize(PP_Resource resource_array) {
  VLOG(4) << "PPB_ResourceArray_Dev::GetSize()";
  EnterResource<PPB_ResourceArray_API> enter(resource_array, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetSize();
}

PP_Resource GetAt(PP_Resource resource_array, uint32_t index) {
  VLOG(4) << "PPB_ResourceArray_Dev::GetAt()";
  EnterResource<PPB_ResourceArray_API> enter(resource_array, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetAt(index);
}

const PPB_ResourceArray_Dev_0_1 g_ppb_resourcearray_dev_thunk_0_1 = {
  &Create,
  &IsResourceArray,
  &GetSize,
  &GetAt
};

}  // namespace

const PPB_ResourceArray_Dev_0_1* GetPPB_ResourceArray_Dev_0_1_Thunk() {
  return &g_ppb_resourcearray_dev_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
