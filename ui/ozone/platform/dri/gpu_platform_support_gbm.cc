// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/gpu_platform_support_gbm.h"

#include "ipc/ipc_message_macros.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/dri/dri_surface_factory.h"
#include "ui/ozone/platform/dri/dri_window_delegate_impl.h"
#include "ui/ozone/platform/dri/dri_window_delegate_manager.h"

namespace ui {

GpuPlatformSupportGbm::GpuPlatformSupportGbm(
    DriSurfaceFactory* dri,
    DriWindowDelegateManager* window_manager,
    ScreenManager* screen_manager)
    : sender_(NULL),
      dri_(dri),
      window_manager_(window_manager),
      screen_manager_(screen_manager) {
}

GpuPlatformSupportGbm::~GpuPlatformSupportGbm() {}

void GpuPlatformSupportGbm::AddHandler(scoped_ptr<GpuPlatformSupport> handler) {
  handlers_.push_back(handler.release());
}

void GpuPlatformSupportGbm::OnChannelEstablished(IPC::Sender* sender) {
  sender_ = sender;

  for (size_t i = 0; i < handlers_.size(); ++i)
    handlers_[i]->OnChannelEstablished(sender);
}

bool GpuPlatformSupportGbm::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(GpuPlatformSupportGbm, message)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_CreateWindowDelegate, OnCreateWindowDelegate)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_DestroyWindowDelegate,
                      OnDestroyWindowDelegate)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_WindowBoundsChanged, OnWindowBoundsChanged)

  IPC_MESSAGE_HANDLER(OzoneGpuMsg_CursorSet, OnCursorSet)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_CursorMove, OnCursorMove)
  IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP()

  if (!handled)
    for (size_t i = 0; i < handlers_.size(); ++i)
      if (handlers_[i]->OnMessageReceived(message))
        return true;

  return false;
}

void GpuPlatformSupportGbm::OnCreateWindowDelegate(
    gfx::AcceleratedWidget widget) {
  // Due to how the GPU process starts up this IPC call may happen after the IPC
  // to create a surface. Since a surface wants to know the window associated
  // with it, we create it ahead of time. So when this call happens we do not
  // create a delegate if it already exists.
  if (!window_manager_->HasWindowDelegate(widget)) {
    scoped_ptr<DriWindowDelegate> delegate(
        new DriWindowDelegateImpl(widget, screen_manager_));
    delegate->Initialize();
    window_manager_->AddWindowDelegate(widget, delegate.Pass());
  }
}

void GpuPlatformSupportGbm::OnDestroyWindowDelegate(
    gfx::AcceleratedWidget widget) {
  scoped_ptr<DriWindowDelegate> delegate =
      window_manager_->RemoveWindowDelegate(widget);
  delegate->Shutdown();
}

void GpuPlatformSupportGbm::OnWindowBoundsChanged(gfx::AcceleratedWidget widget,
                                                  const gfx::Rect& bounds) {
  window_manager_->GetWindowDelegate(widget)->OnBoundsChanged(bounds);
}

void GpuPlatformSupportGbm::OnCursorSet(gfx::AcceleratedWidget widget,
                                        const std::vector<SkBitmap>& bitmaps,
                                        const gfx::Point& location,
                                        int frame_delay_ms) {
  dri_->SetHardwareCursor(widget, bitmaps, location, frame_delay_ms);
}

void GpuPlatformSupportGbm::OnCursorMove(gfx::AcceleratedWidget widget,
                                         const gfx::Point& location) {
  dri_->MoveHardwareCursor(widget, location);
}

}  // namespace ui
