// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_CURSOR_CONTROL_API_H_
#define PPAPI_THUNK_CURSOR_CONTROL_API_H_

#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/proxy/interface_id.h"

namespace ppapi {
namespace thunk {

class PPB_CursorControl_FunctionAPI {
 public:
  virtual ~PPB_CursorControl_FunctionAPI() {}

  virtual PP_Bool SetCursor(PP_Instance instance,
                            PP_CursorType_Dev type,
                            PP_Resource custom_image_id,
                            const PP_Point* hot_spot) = 0;
  virtual PP_Bool LockCursor(PP_Instance instance) = 0;
  virtual PP_Bool UnlockCursor(PP_Instance instance) = 0;
  virtual PP_Bool HasCursorLock(PP_Instance instance) = 0;
  virtual PP_Bool CanLockCursor(PP_Instance instance) = 0;

  static const proxy::InterfaceID interface_id =
      proxy::INTERFACE_ID_PPB_CURSORCONTROL;

};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_CURSOR_CONTROL_API_H_
