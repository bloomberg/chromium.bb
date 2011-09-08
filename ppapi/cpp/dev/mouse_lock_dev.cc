// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/mouse_lock_dev.h"

#include "ppapi/c/dev/ppb_mouse_lock_dev.h"
#include "ppapi/c/dev/ppp_mouse_lock_dev.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

static const char kPPPMouseLockInterface[] = PPP_MOUSELOCK_DEV_INTERFACE;

void MouseLockLost(PP_Instance instance) {
  void* object =
      pp::Instance::GetPerInstanceObject(instance, kPPPMouseLockInterface);
  if (!object)
    return;
  static_cast<MouseLock_Dev*>(object)->MouseLockLost();
}

const PPP_MouseLock_Dev ppp_mouse_lock = {
  &MouseLockLost
};

template <> const char* interface_name<PPB_MouseLock_Dev>() {
  return PPB_MOUSELOCK_DEV_INTERFACE;
}

}  // namespace

MouseLock_Dev::MouseLock_Dev(Instance* instance)
    : associated_instance_(instance) {
  pp::Module::Get()->AddPluginInterface(kPPPMouseLockInterface,
                                        &ppp_mouse_lock);
  associated_instance_->AddPerInstanceObject(kPPPMouseLockInterface, this);
}

MouseLock_Dev::~MouseLock_Dev() {
  associated_instance_->RemovePerInstanceObject(kPPPMouseLockInterface, this);
}

int32_t MouseLock_Dev::LockMouse(const CompletionCallback& cc) {
  if (!has_interface<PPB_MouseLock_Dev>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_MouseLock_Dev>()->LockMouse(
      associated_instance_->pp_instance(), cc.pp_completion_callback());
}

void MouseLock_Dev::UnlockMouse() {
  if (has_interface<PPB_MouseLock_Dev>()) {
    get_interface<PPB_MouseLock_Dev>()->UnlockMouse(
        associated_instance_->pp_instance());
  }
}

}  // namespace pp
