// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/ppb_mouse_lock_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/common.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

int32_t LockMouse(PP_Instance instance, PP_CompletionCallback callback) {
  EnterFunction<PPB_Instance_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADARGUMENT);
  int32_t result = enter.functions()->LockMouse(instance, callback);
  return MayForceCallback(callback, result);
}

void UnlockMouse(PP_Instance instance) {
  EnterFunction<PPB_Instance_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return;
  enter.functions()->UnlockMouse(instance);
}

const PPB_MouseLock_Dev g_ppb_mouse_lock_thunk = {
  &LockMouse,
  &UnlockMouse
};

}  // namespace

const PPB_MouseLock_Dev* GetPPB_MouseLock_Dev_Thunk() {
  return &g_ppb_mouse_lock_thunk;
}

}  // namespace thunk
}  // namespace ppapi
