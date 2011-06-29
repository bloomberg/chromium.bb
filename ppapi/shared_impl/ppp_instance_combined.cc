// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppp_instance_combined.h"

namespace ppapi {

PPP_Instance_Combined::PPP_Instance_Combined(
    const PPP_Instance_0_5& instance_if)
    : PPP_Instance_0_5(instance_if),
      GetInstanceObject_0_4(NULL) {
}

PPP_Instance_Combined::PPP_Instance_Combined(
    const PPP_Instance_0_4& instance_if)
    : PPP_Instance_0_5(),  // Zero-initialize.
      GetInstanceObject_0_4(instance_if.GetInstanceObject) {
  DidCreate = instance_if.DidCreate;
  DidDestroy = instance_if.DidDestroy;
  DidChangeView = instance_if.DidChangeView;
  DidChangeFocus = instance_if.DidChangeFocus;
  HandleInputEvent = instance_if.HandleInputEvent;
  HandleDocumentLoad = instance_if.HandleDocumentLoad;
}

}  // namespace ppapi

