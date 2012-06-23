// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_mouse_lock.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

int32_t LockMouse(PP_Instance instance, PP_CompletionCallback callback) {
  EnterInstance enter(instance, callback);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.functions()->LockMouse(instance,
                                                      enter.callback()));
}

void UnlockMouse(PP_Instance instance) {
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->UnlockMouse(instance);
}

const PPB_MouseLock g_ppb_mouse_lock_thunk = {
  &LockMouse,
  &UnlockMouse
};

}  // namespace

const PPB_MouseLock_1_0* GetPPB_MouseLock_1_0_Thunk() {
  return &g_ppb_mouse_lock_thunk;
}

}  // namespace thunk
}  // namespace ppapi
