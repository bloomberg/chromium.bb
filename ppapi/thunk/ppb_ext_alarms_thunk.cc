// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ppapi/c/extensions/dev/ppb_ext_alarms_dev.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/extensions_common_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

void Create(PP_Instance instance,
            PP_Var name,
            PP_Ext_Alarms_AlarmCreateInfo_Dev alarm_info) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance);
  if (enter.failed())
    return;

  std::vector<PP_Var> args;
  args.push_back(name);
  args.push_back(alarm_info);
  enter.functions()->Post("alarms.create", args);
}

int32_t Get(PP_Instance instance,
            PP_Var name,
            PP_Ext_Alarms_Alarm_Dev* alarm,
            PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  std::vector<PP_Var> input_args;
  std::vector<PP_Var*> output_args;
  input_args.push_back(name);
  output_args.push_back(alarm);
  return enter.SetResult(enter.functions()->Call(
      "alarms.get", input_args, output_args, enter.callback()));
}

int32_t GetAll(PP_Instance instance,
               PP_Ext_Alarms_Alarm_Dev_Array* alarms,
               PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  std::vector<PP_Var> input_args;
  std::vector<PP_Var*> output_args;
  output_args.push_back(alarms);
  return enter.SetResult(enter.functions()->Call(
      "alarms.getAll", input_args, output_args, enter.callback()));
}

void Clear(PP_Instance instance, PP_Var name) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance);
  if (enter.failed())
    return;

  std::vector<PP_Var> args;
  args.push_back(name);
  enter.functions()->Post("alarms.clear", args);
}

void ClearAll(PP_Instance instance) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance);
  if (enter.failed())
    return;

  std::vector<PP_Var> args;
  enter.functions()->Post("alarms.clearAll", args);
}

const PPB_Ext_Alarms_Dev_0_1 g_ppb_ext_alarms_dev_0_1_thunk = {
  &Create,
  &Get,
  &GetAll,
  &Clear,
  &ClearAll
};

}  // namespace

const PPB_Ext_Alarms_Dev_0_1* GetPPB_Ext_Alarms_Dev_0_1_Thunk() {
  return &g_ppb_ext_alarms_dev_0_1_thunk;
}

}  // namespace thunk
}  // namespace ppapi
