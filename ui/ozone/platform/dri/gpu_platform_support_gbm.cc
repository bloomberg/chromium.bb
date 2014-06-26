// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/gpu_platform_support_gbm.h"

#include "ipc/ipc_message_macros.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/dri/dri_surface_factory.h"

namespace ui {

GpuPlatformSupportGbm::GpuPlatformSupportGbm(DriSurfaceFactory* dri)
    : sender_(NULL), dri_(dri) {
}

void GpuPlatformSupportGbm::OnChannelEstablished(IPC::Sender* sender) {
  sender_ = sender;
}

bool GpuPlatformSupportGbm::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(GpuPlatformSupportGbm, message)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_CursorSet, OnCursorSet)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_CursorMove, OnCursorMove)
  IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP()

  return handled;
}

void GpuPlatformSupportGbm::OnCursorSet(gfx::AcceleratedWidget widget,
                                        const SkBitmap& bitmap,
                                        const gfx::Point& location) {
  dri_->SetHardwareCursor(widget, bitmap, location);
}

void GpuPlatformSupportGbm::OnCursorMove(gfx::AcceleratedWidget widget,
                                         const gfx::Point& location) {
  dri_->MoveHardwareCursor(widget, location);
}

}  // namespace ui
