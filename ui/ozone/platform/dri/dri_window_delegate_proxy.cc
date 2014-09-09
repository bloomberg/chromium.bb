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
  TRACE_EVENT1("dri", "DriWindowDelegateProxy::Initialize", "widget", widget_);
  sender_->AddChannelObserver(this);
}

void DriWindowDelegateProxy::Shutdown() {
  TRACE_EVENT1("dri", "DriWindowDelegateProxy::Shutdown", "widget", widget_);
  sender_->RemoveChannelObserver(this);
  if (!sender_->IsConnected())
    return;

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
  TRACE_EVENT2("dri",
               "DriWindowDelegateProxy::OnBoundsChanged",
               "widget",
               widget_,
               "bounds",
               bounds.ToString());
  bounds_ = bounds;
  if (!sender_->IsConnected())
    return;

  bool status =
      sender_->Send(new OzoneGpuMsg_WindowBoundsChanged(widget_, bounds));
  DCHECK(status);
}

void DriWindowDelegateProxy::OnChannelEstablished() {
  TRACE_EVENT1(
      "dri", "DriWindowDelegateProxy::OnChannelEstablished", "widget", widget_);
  bool status = sender_->Send(new OzoneGpuMsg_CreateWindowDelegate(widget_));
  DCHECK(status);
  OnBoundsChanged(bounds_);
}

void DriWindowDelegateProxy::OnChannelDestroyed() {
  TRACE_EVENT1(
      "dri", "DriWindowDelegateProxy::OnChannelDestroyed", "widget", widget_);
}

}  // namespace ui
