// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_cursor_control_impl.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "ppapi/c/dev/pp_cursor_type_dev.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_resource.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

using ::ppapi::thunk::PPB_CursorControl_FunctionAPI;

namespace webkit {
namespace ppapi {

PPB_CursorControl_Impl::PPB_CursorControl_Impl(PluginInstance* instance)
    : instance_(instance) {
}

PPB_CursorControl_Impl::~PPB_CursorControl_Impl() {
}

PPB_CursorControl_FunctionAPI*
PPB_CursorControl_Impl::AsPPB_CursorControl_FunctionAPI() {
  return this;
}

PP_Bool PPB_CursorControl_Impl::SetCursor(PP_Instance instance,
                                          PP_CursorType_Dev type,
                                          PP_Resource custom_image_id,
                                          const PP_Point* hot_spot) {
  return PP_FromBool(instance_->SetCursor(type, custom_image_id, hot_spot));
}

PP_Bool PPB_CursorControl_Impl::LockCursor(PP_Instance instance) {
  // TODO: implement cursor locking.
  return PP_FALSE;
}

PP_Bool PPB_CursorControl_Impl::UnlockCursor(PP_Instance instance) {
  // TODO: implement cursor locking.
  return PP_FALSE;
}

PP_Bool PPB_CursorControl_Impl::HasCursorLock(PP_Instance instance) {
  // TODO: implement cursor locking.
  return PP_FALSE;
}

PP_Bool PPB_CursorControl_Impl::CanLockCursor(PP_Instance instance) {
  // TODO: implement cursor locking.
  return PP_FALSE;
}

}  // namespace ppapi
}  // namespace webkit
