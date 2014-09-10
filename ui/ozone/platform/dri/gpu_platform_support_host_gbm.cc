// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/gpu_platform_support_host_gbm.h"

#include "base/debug/trace_event.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/dri/channel_observer.h"

namespace ui {

GpuPlatformSupportHostGbm::GpuPlatformSupportHostGbm()
    : host_id_(-1), sender_(NULL) {
}

GpuPlatformSupportHostGbm::~GpuPlatformSupportHostGbm() {}

bool GpuPlatformSupportHostGbm::IsConnected() const {
  return sender_ != NULL;
}

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

void GpuPlatformSupportHostGbm::AddChannelObserver(ChannelObserver* observer) {
  channel_observers_.AddObserver(observer);

  if (sender_)
    observer->OnChannelEstablished();
}

void GpuPlatformSupportHostGbm::RemoveChannelObserver(
    ChannelObserver* observer) {
  channel_observers_.RemoveObserver(observer);
}

void GpuPlatformSupportHostGbm::OnChannelEstablished(int host_id,
                                                     IPC::Sender* sender) {
  TRACE_EVENT1("dri",
               "GpuPlatformSupportHostGbm::OnChannelEstablished",
               "host_id",
               host_id);
  host_id_ = host_id;
  sender_ = sender;

  while (!queued_messages_.empty()) {
    Send(queued_messages_.front());
    queued_messages_.pop();
  }

  for (size_t i = 0; i < handlers_.size(); ++i)
    handlers_[i]->OnChannelEstablished(host_id, sender);

  FOR_EACH_OBSERVER(
      ChannelObserver, channel_observers_, OnChannelEstablished());
}

void GpuPlatformSupportHostGbm::OnChannelDestroyed(int host_id) {
  TRACE_EVENT1("dri",
               "GpuPlatformSupportHostGbm::OnChannelDestroyed",
               "host_id",
               host_id);
  if (host_id_ == host_id) {
    host_id_ = -1;
    sender_ = NULL;

    FOR_EACH_OBSERVER(
        ChannelObserver, channel_observers_, OnChannelDestroyed());
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

void GpuPlatformSupportHostGbm::SetHardwareCursor(
    gfx::AcceleratedWidget widget,
    const std::vector<SkBitmap>& bitmaps,
    const gfx::Point& location,
    int frame_delay_ms) {
  Send(new OzoneGpuMsg_CursorSet(widget, bitmaps, location, frame_delay_ms));
}

void GpuPlatformSupportHostGbm::MoveHardwareCursor(
    gfx::AcceleratedWidget widget,
    const gfx::Point& location) {
  Send(new OzoneGpuMsg_CursorMove(widget, location));
}

}  // namespace ui
