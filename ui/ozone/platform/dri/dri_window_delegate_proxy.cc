// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_window_delegate_proxy.h"

#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/dri/gpu_platform_support_host_gbm.h"

namespace ui {

DriWindowDelegateProxy::DriWindowDelegateProxy(
    gfx::AcceleratedWidget widget,
    GpuPlatformSupportHostGbm* sender)
    : widget_(widget), sender_(sender) {
}

DriWindowDelegateProxy::~DriWindowDelegateProxy() {
}

void DriWindowDelegateProxy::Initialize() {
  bool status = sender_->Send(new OzoneGpuMsg_CreateWindowDelegate(widget_));
  DCHECK(status);
}

void DriWindowDelegateProxy::Shutdown() {
  bool status = sender_->Send(new OzoneGpuMsg_DestroyWindowDelegate(widget_));
  DCHECK(status);
}

gfx::AcceleratedWidget DriWindowDelegateProxy::GetAcceleratedWidget() {
  return widget_;
}

HardwareDisplayController* DriWindowDelegateProxy::GetController() {
  NOTREACHED();
  return NULL;
}

void DriWindowDelegateProxy::OnBoundsChanged(const gfx::Rect& bounds) {
  bool status =
      sender_->Send(new OzoneGpuMsg_WindowBoundsChanged(widget_, bounds));
  DCHECK(status);
}

}  // namespace ui
