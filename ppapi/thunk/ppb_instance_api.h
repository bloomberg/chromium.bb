// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_INSTANCE_API_H_
#define PPAPI_THUNK_INSTANCE_API_H_

#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/private/ppb_instance_private.h"
#include "ppapi/proxy/interface_id.h"

namespace ppapi {
namespace thunk {

class PPB_Instance_FunctionAPI {
 public:
  static const ::pp::proxy::InterfaceID interface_id =
      ::pp::proxy::INTERFACE_ID_PPB_INSTANCE;

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
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_INSTANCE_API_H_
