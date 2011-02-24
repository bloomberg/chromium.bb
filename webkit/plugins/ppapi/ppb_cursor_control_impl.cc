// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_cursor_control_impl.h"

#include "base/logging.h"
#include "base/ref_counted.h"
#include "ppapi/c/dev/pp_cursor_type_dev.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_resource.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource.h"

namespace webkit {
namespace ppapi {

namespace {

PP_Bool SetCursor(PP_Instance instance_id,
                  PP_CursorType_Dev type,
                  PP_Resource custom_image_id,
                  const PP_Point* hot_spot) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return PP_FALSE;

  scoped_refptr<PPB_ImageData_Impl> custom_image(
      Resource::GetAs<PPB_ImageData_Impl>(custom_image_id));
  if (custom_image.get()) {
    // TODO(neb): implement custom cursors.
    // (Remember that PP_CURSORTYPE_CUSTOM != WebCursorInfo::TypeCustom.)
    NOTIMPLEMENTED();
    return PP_FALSE;
  }

  return BoolToPPBool(instance->SetCursor(type));
}

PP_Bool LockCursor(PP_Instance instance_id) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return PP_FALSE;

  // TODO(neb): implement cursor locking.
  return PP_FALSE;
}

PP_Bool UnlockCursor(PP_Instance instance_id) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return PP_FALSE;

  // TODO(neb): implement cursor locking.
  return PP_FALSE;
}

PP_Bool HasCursorLock(PP_Instance instance_id) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return PP_FALSE;

  // TODO(neb): implement cursor locking.
  return PP_FALSE;
}

PP_Bool CanLockCursor(PP_Instance instance_id) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return PP_FALSE;

  // TODO(neb): implement cursor locking.
  return PP_FALSE;
}

const PPB_CursorControl_Dev cursor_control_interface = {
  &SetCursor,
  &LockCursor,
  &UnlockCursor,
  &HasCursorLock,
  &CanLockCursor
};

}  // namespace

const PPB_CursorControl_Dev* GetCursorControlInterface() {
  return &cursor_control_interface;
}

}  // namespace ppapi
}  // namespace webkit

