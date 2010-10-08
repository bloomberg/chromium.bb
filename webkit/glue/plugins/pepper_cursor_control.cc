// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_cursor_control.h"

#include "base/logging.h"
#include "base/ref_counted.h"
#include "third_party/ppapi/c/dev/pp_cursor_type_dev.h"
#include "third_party/ppapi/c/dev/ppb_cursor_control_dev.h"
#include "third_party/ppapi/c/pp_point.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "webkit/glue/plugins/pepper_image_data.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_resource.h"

namespace pepper {

namespace {

bool SetCursor(PP_Instance instance_id,
               PP_CursorType_Dev type,
               PP_Resource custom_image_id,
               const PP_Point* hot_spot) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return false;

  scoped_refptr<ImageData> custom_image(
      Resource::GetAs<ImageData>(custom_image_id));
  if (custom_image.get()) {
    // TODO(neb): implement custom cursors.
    NOTIMPLEMENTED();
    return false;
  }

  return instance->SetCursor(type);
}

bool LockCursor(PP_Instance instance_id) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return false;

  // TODO(neb): implement cursor locking.
  return false;
}

bool UnlockCursor(PP_Instance instance_id) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return false;

  // TODO(neb): implement cursor locking.
  return false;
}

bool HasCursorLock(PP_Instance instance_id) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return false;

  // TODO(neb): implement cursor locking.
  return false;
}

bool CanLockCursor(PP_Instance instance_id) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return false;

  // TODO(neb): implement cursor locking.
  return false;
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

}  // namespace pepper

