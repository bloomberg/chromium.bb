// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/ppb_query_policy_dev.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"

namespace ppapi {
namespace thunk {

namespace {

void SubscribeToPolicyUpdates(PP_Instance instance) {
  EnterFunction<PPB_Instance_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return;
  enter.functions()->SubscribeToPolicyUpdates(instance);
}

const PPB_QueryPolicy_Dev g_ppb_querypolicy_thunk = {
  &SubscribeToPolicyUpdates,
};

}  // namespace

const PPB_QueryPolicy_Dev* GetPPB_QueryPolicy_Dev_Thunk() {
  return &g_ppb_querypolicy_thunk;
}

}  // namespace thunk
}  // namespace ppapi
