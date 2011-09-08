// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_INSTANCE_API_H_
#define PPAPI_THUNK_INSTANCE_API_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/private/ppb_instance_private.h"
#include "ppapi/proxy/interface_id.h"

namespace ppapi {
namespace thunk {

class PPB_Instance_FunctionAPI {
 public:
  virtual ~PPB_Instance_FunctionAPI() {}

  virtual PP_Bool BindGraphics(PP_Instance instance, PP_Resource device) = 0;
  virtual PP_Bool IsFullFrame(PP_Instance instance) = 0;

  // InstancePrivate.
  virtual PP_Var GetWindowObject(PP_Instance instance) = 0;
  virtual PP_Var GetOwnerElementObject(PP_Instance instance) = 0;
  virtual PP_Var ExecuteScript(PP_Instance instance,
                               PP_Var script,
                               PP_Var* exception) = 0;

  // Fullscreen.
  virtual PP_Bool IsFullscreen(PP_Instance instance) = 0;
  virtual PP_Bool SetFullscreen(PP_Instance instance, PP_Bool fullscreen) = 0;
  virtual PP_Bool GetScreenSize(PP_Instance instance, PP_Size* size) = 0;

  // InputEvent.
  virtual int32_t RequestInputEvents(PP_Instance instance,
                                     uint32_t event_classes) = 0;
  virtual int32_t RequestFilteringInputEvents(PP_Instance instance,
                                              uint32_t event_classes) = 0;
  virtual void ClearInputEventRequest(PP_Instance instance,
                                      uint32_t event_classes) = 0;

  // Messaging.
  virtual void PostMessage(PP_Instance instance, PP_Var message) = 0;

  // MouseLock.
  virtual int32_t LockMouse(PP_Instance instance,
                            PP_CompletionCallback callback) = 0;
  virtual void UnlockMouse(PP_Instance instance) = 0;

  // Zoom.
  virtual void ZoomChanged(PP_Instance instance, double factor) = 0;
  virtual void ZoomLimitsChanged(PP_Instance instance,
                                 double minimum_factor,
                                 double maximium_factor) = 0;

  // QueryPolicy.
  virtual void SubscribeToPolicyUpdates(PP_Instance instance) = 0;

  static const proxy::InterfaceID interface_id =
      proxy::INTERFACE_ID_PPB_INSTANCE;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_INSTANCE_API_H_
