// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/gpu_platform_support_host_gbm.h"

#include "base/debug/trace_event.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"

namespace ui {

GpuPlatformSupportHostGbm::GpuPlatformSupportHostGbm()
    : host_id_(-1), sender_(NULL) {
}

GpuPlatformSupportHostGbm::~GpuPlatformSupportHostGbm() {}

void GpuPlatformSupportHostGbm::RegisterHandler(
    GpuPlatformSupportHost* handler) {
  handlers_.push_back(handler);
}

void GpuPlatformSupportHostGbm::UnregisterHandler(
    GpuPlatformSupportHost* handler) {
  std::vector<GpuPlatformSupportHost*>::iterator it =
      std::find(handlers_.begin(), handlers_.end(), handler);
  if (it != handlers_.end())
    handlers_.erase(it);
}

void GpuPlatformSupportHostGbm::OnChannelEstablished(int host_id,
                                                     IPC::Sender* sender) {
  TRACE_EVENT0("dri", "GpuPlatformSupportHostGbm::OnChannelEstablished");
  host_id_ = host_id;
  sender_ = sender;

  while (!queued_messages_.empty()) {
    Send(queued_messages_.front());
    queued_messages_.pop();
  }

  for (size_t i = 0; i < handlers_.size(); ++i)
    handlers_[i]->OnChannelEstablished(host_id, sender);
}

void GpuPlatformSupportHostGbm::OnChannelDestroyed(int host_id) {
  TRACE_EVENT0("dri", "GpuPlatformSupportHostGbm::OnChannelDestroyed");
  if (host_id_ == host_id) {
    host_id_ = -1;
    sender_ = NULL;
  }

  for (size_t i = 0; i < handlers_.size(); ++i)
    handlers_[i]->OnChannelDestroyed(host_id);
}

bool GpuPlatformSupportHostGbm::OnMessageReceived(const IPC::Message& message) {
  for (size_t i = 0; i < handlers_.size(); ++i)
    if (handlers_[i]->OnMessageReceived(message))
      return true;

  return false;
}

bool GpuPlatformSupportHostGbm::Send(IPC::Message* message) {
  if (sender_)
    return sender_->Send(message);

  queued_messages_.push(message);
  return true;
}

void GpuPlatformSupportHostGbm::SetHardwareCursor(gfx::AcceleratedWidget widget,
                                                  const SkBitmap& bitmap,
                                                  const gfx::Point& location) {
  Send(new OzoneGpuMsg_CursorSet(widget, bitmap, location));
}

void GpuPlatformSupportHostGbm::MoveHardwareCursor(
    gfx::AcceleratedWidget widget,
    const gfx::Point& location) {
  Send(new OzoneGpuMsg_CursorMove(widget, location));
}

}  // namespace ui
