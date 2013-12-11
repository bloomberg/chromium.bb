// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "ppapi/c/dev/ppb_alarms_dev.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/extensions_common_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

// TODO(yzshen): crbug.com/327197 Implement the thunk.

void Create(PP_Instance instance,
            PP_Var name,
            const PP_Alarms_AlarmCreateInfo_Dev* alarm_info) {
  NOTIMPLEMENTED();
}

int32_t Get(PP_Instance instance,
            PP_Var name,
            PP_Alarms_Alarm_Dev* alarm,
            PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  NOTIMPLEMENTED();

  return enter.SetResult(PP_ERROR_FAILED);
}

int32_t GetAll(PP_Instance instance,
               PP_Alarms_Alarm_Array_Dev* alarms,
               PP_ArrayOutput array_allocator,
               PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  NOTIMPLEMENTED();

  return enter.SetResult(PP_ERROR_FAILED);
}

void Clear(PP_Instance instance, PP_Var name) {
  NOTIMPLEMENTED();
}

void ClearAll(PP_Instance instance) {
  NOTIMPLEMENTED();
}

uint32_t AddOnAlarmListener(PP_Instance instance,
                            PP_Alarms_OnAlarm_Dev callback,
                            void* user_data) {
  NOTIMPLEMENTED();
  return 0;
}

const PPB_Alarms_Dev_0_1 g_ppb_alarms_dev_0_1_thunk = {
  &Create,
  &Get,
  &GetAll,
  &Clear,
  &ClearAll,
  &AddOnAlarmListener
};

}  // namespace

const PPB_Alarms_Dev_0_1* GetPPB_Alarms_Dev_0_1_Thunk() {
  return &g_ppb_alarms_dev_0_1_thunk;
}

}  // namespace thunk
}  // namespace ppapi
