// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_cursor_control_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool SetCursor(PP_Instance instance,
                  PP_CursorType_Dev type,
                  PP_Resource custom_image,
                  const PP_Point* hot_spot) {
  EnterFunction<PPB_CursorControl_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->SetCursor(instance, type, custom_image, hot_spot);
}

PP_Bool LockCursor(PP_Instance instance) {
  EnterFunction<PPB_CursorControl_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->LockCursor(instance);
}

PP_Bool UnlockCursor(PP_Instance instance) {
  EnterFunction<PPB_CursorControl_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->UnlockCursor(instance);
}

PP_Bool HasCursorLock(PP_Instance instance) {
  EnterFunction<PPB_CursorControl_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->HasCursorLock(instance);
}

PP_Bool CanLockCursor(PP_Instance instance) {
  EnterFunction<PPB_CursorControl_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->CanLockCursor(instance);
}

const PPB_CursorControl_Dev g_ppb_cursor_control_thunk = {
  &SetCursor,
  &LockCursor,
  &UnlockCursor,
  &HasCursorLock,
  &CanLockCursor
};

}  // namespace

const PPB_CursorControl_Dev* GetPPB_CursorControl_Dev_Thunk() {
  return &g_ppb_cursor_control_thunk;
}

}  // namespace thunk
}  // namespace ppapi
