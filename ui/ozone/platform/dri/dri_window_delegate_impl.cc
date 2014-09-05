// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_window_delegate_impl.h"

#include "base/debug/trace_event.h"
#include "ui/ozone/platform/dri/screen_manager.h"

namespace ui {

DriWindowDelegateImpl::DriWindowDelegateImpl(gfx::AcceleratedWidget widget,
                                             ScreenManager* screen_manager)
    : widget_(widget), screen_manager_(screen_manager) {
}

DriWindowDelegateImpl::~DriWindowDelegateImpl() {
}

void DriWindowDelegateImpl::Initialize() {
  TRACE_EVENT1("dri", "DriWindowDelegateImpl::Initialize", "widget", widget_);
}

void DriWindowDelegateImpl::Shutdown() {
  TRACE_EVENT1("dri", "DriWindowDelegateImpl::Shutdown", "widget", widget_);
}

gfx::AcceleratedWidget DriWindowDelegateImpl::GetAcceleratedWidget() {
  return widget_;
}

HardwareDisplayController* DriWindowDelegateImpl::GetController() {
  return controller_.get();
}

void DriWindowDelegateImpl::OnBoundsChanged(const gfx::Rect& bounds) {
  TRACE_EVENT2("dri",
               "DriWindowDelegateImpl::OnBoundsChanged",
               "widget",
               widget_,
               "bounds",
               bounds.ToString());
  controller_ = screen_manager_->GetDisplayController(bounds);
}

}  // namespace ui
