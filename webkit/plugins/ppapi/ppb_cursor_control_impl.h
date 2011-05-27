// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_CURSOR_CONTROL_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_CURSOR_CONTROL_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/shared_impl/function_group_base.h"
#include "ppapi/thunk/ppb_cursor_control_api.h"

namespace webkit {
namespace ppapi {

class PluginInstance;

class PPB_CursorControl_Impl
    : public ::ppapi::FunctionGroupBase,
      public ::ppapi::thunk::PPB_CursorControl_FunctionAPI {
 public:
  PPB_CursorControl_Impl(PluginInstance* instance);
  ~PPB_CursorControl_Impl();

  // FunctionGroupBase overrides.
  virtual ::ppapi::thunk::PPB_CursorControl_FunctionAPI*
      AsCursorControl_FunctionAPI() OVERRIDE;

  // PPB_CursorControl_FunctionAPI implementation.
  virtual PP_Bool SetCursor(PP_Instance instance,
                            PP_CursorType_Dev type,
                            PP_Resource custom_image_id,
                            const PP_Point* hot_spot) OVERRIDE;
  virtual PP_Bool LockCursor(PP_Instance instance) OVERRIDE;
  virtual PP_Bool UnlockCursor(PP_Instance instance) OVERRIDE;
  virtual PP_Bool HasCursorLock(PP_Instance instance) OVERRIDE;
  virtual PP_Bool CanLockCursor(PP_Instance instance) OVERRIDE;

 private:
  PluginInstance* instance_;

  DISALLOW_COPY_AND_ASSIGN(PPB_CursorControl_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_CURSOR_CONTROL_IMPL_H_

