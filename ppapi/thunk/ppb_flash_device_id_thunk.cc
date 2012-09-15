// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/private/ppb_flash_device_id.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_flash_device_id_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateFlashDeviceID(instance);
}

int32_t GetDeviceID(PP_Resource resource,
                    PP_Var* id,
                    PP_CompletionCallback callback) {
  EnterResource<PPB_Flash_DeviceID_API> enter(resource, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->GetDeviceID(id, enter.callback()));
}

const PPB_Flash_DeviceID g_ppb_flash_deviceid_thunk = {
  &Create,
  &GetDeviceID
};

}  // namespace

const PPB_Flash_DeviceID_1_0* GetPPB_Flash_DeviceID_1_0_Thunk() {
  return &g_ppb_flash_deviceid_thunk;
}

}  // namespace thunk
}  // namespace ppapi
