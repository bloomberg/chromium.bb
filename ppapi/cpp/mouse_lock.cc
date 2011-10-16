// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/mouse_lock.h"

#include "ppapi/c/ppb_mouse_lock.h"
#include "ppapi/c/ppp_mouse_lock.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

static const char kPPPMouseLockInterface[] = PPP_MOUSELOCK_INTERFACE;

void MouseLockLost(PP_Instance instance) {
  void* object =
      pp::Instance::GetPerInstanceObject(instance, kPPPMouseLockInterface);
  if (!object)
    return;
  static_cast<MouseLock*>(object)->MouseLockLost();
}

const PPP_MouseLock ppp_mouse_lock = {
  &MouseLockLost
};

template <> const char* interface_name<PPB_MouseLock>() {
  return PPB_MOUSELOCK_INTERFACE;
}

}  // namespace

MouseLock::MouseLock(Instance* instance)
    : associated_instance_(instance) {
  pp::Module::Get()->AddPluginInterface(kPPPMouseLockInterface,
                                        &ppp_mouse_lock);
  associated_instance_->AddPerInstanceObject(kPPPMouseLockInterface, this);
}

MouseLock::~MouseLock() {
  associated_instance_->RemovePerInstanceObject(kPPPMouseLockInterface, this);
}

int32_t MouseLock::LockMouse(const CompletionCallback& cc) {
  if (!has_interface<PPB_MouseLock>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_MouseLock>()->LockMouse(
      associated_instance_->pp_instance(), cc.pp_completion_callback());
}

void MouseLock::UnlockMouse() {
  if (has_interface<PPB_MouseLock>()) {
    get_interface<PPB_MouseLock>()->UnlockMouse(
        associated_instance_->pp_instance());
  }
}

}  // namespace pp
