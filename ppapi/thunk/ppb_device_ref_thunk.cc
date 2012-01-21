// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_device_ref_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool IsDeviceRef(PP_Resource resource) {
  EnterResource<PPB_DeviceRef_API> enter(resource, false);
  return enter.succeeded() ? PP_TRUE : PP_FALSE;
}

PP_DeviceType_Dev GetType(PP_Resource device_ref) {
  EnterResource<PPB_DeviceRef_API> enter(device_ref, true);
  if (enter.failed())
    return PP_DEVICETYPE_DEV_INVALID;
  return enter.object()->GetType();
}

PP_Var GetName(PP_Resource device_ref) {
  EnterResource<PPB_DeviceRef_API> enter(device_ref, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetName();
}

const PPB_DeviceRef_Dev g_ppb_device_ref_thunk = {
  &IsDeviceRef,
  &GetType,
  &GetName
};

}  // namespace

const PPB_DeviceRef_Dev_0_1* GetPPB_DeviceRef_Dev_0_1_Thunk() {
  return &g_ppb_device_ref_thunk;
}

}  // namespace thunk
}  // namespace ppapi
