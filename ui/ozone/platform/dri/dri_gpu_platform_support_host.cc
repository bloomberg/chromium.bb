// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_gpu_platform_support_host.h"

#include "base/debug/trace_event.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/dri/channel_observer.h"

namespace ui {

DriGpuPlatformSupportHost::DriGpuPlatformSupportHost()
    : host_id_(-1), sender_(NULL) {
}

DriGpuPlatformSupportHost::~DriGpuPlatformSupportHost() {
}

bool DriGpuPlatformSupportHost::IsConnected() const {
  return sender_ != NULL;
}

void DriGpuPlatformSupportHost::RegisterHandler(
    GpuPlatformSupportHost* handler) {
  handlers_.push_back(handler);
}

void DriGpuPlatformSupportHost::UnregisterHandler(
    GpuPlatformSupportHost* handler) {
  std::vector<GpuPlatformSupportHost*>::iterator it =
      std::find(handlers_.begin(), handlers_.end(), handler);
  if (it != handlers_.end())
    handlers_.erase(it);
}

void DriGpuPlatformSupportHost::AddChannelObserver(ChannelObserver* observer) {
  channel_observers_.AddObserver(observer);

  if (sender_)
    observer->OnChannelEstablished();
}

void DriGpuPlatformSupportHost::RemoveChannelObserver(
    ChannelObserver* observer) {
  channel_observers_.RemoveObserver(observer);
}

void DriGpuPlatformSupportHost::OnChannelEstablished(int host_id,
                                                     IPC::Sender* sender) {
  TRACE_EVENT1("dri", "DriGpuPlatformSupportHost::OnChannelEstablished",
               "host_id", host_id);
  host_id_ = host_id;
  sender_ = sender;

  while (!queued_messages_.empty()) {
    Send(queued_messages_.front());
    queued_messages_.pop();
  }

  for (size_t i = 0; i < handlers_.size(); ++i)
    handlers_[i]->OnChannelEstablished(host_id, sender);

  FOR_EACH_OBSERVER(ChannelObserver, channel_observers_,
                    OnChannelEstablished());
}

void DriGpuPlatformSupportHost::OnChannelDestroyed(int host_id) {
  TRACE_EVENT1("dri", "DriGpuPlatformSupportHost::OnChannelDestroyed",
               "host_id", host_id);
  if (host_id_ == host_id) {
    host_id_ = -1;
    sender_ = NULL;

    FOR_EACH_OBSERVER(ChannelObserver, channel_observers_,
                      OnChannelDestroyed());
  }

  for (size_t i = 0; i < handlers_.size(); ++i)
    handlers_[i]->OnChannelDestroyed(host_id);
}

bool DriGpuPlatformSupportHost::OnMessageReceived(const IPC::Message& message) {
  for (size_t i = 0; i < handlers_.size(); ++i)
    if (handlers_[i]->OnMessageReceived(message))
      return true;

  return false;
}

bool DriGpuPlatformSupportHost::Send(IPC::Message* message) {
  if (sender_)
    return sender_->Send(message);

  queued_messages_.push(message);
  return true;
}

void DriGpuPlatformSupportHost::SetHardwareCursor(
    gfx::AcceleratedWidget widget,
    const std::vector<SkBitmap>& bitmaps,
    const gfx::Point& location,
    int frame_delay_ms) {
  Send(new OzoneGpuMsg_CursorSet(widget, bitmaps, location, frame_delay_ms));
}

void DriGpuPlatformSupportHost::MoveHardwareCursor(
    gfx::AcceleratedWidget widget,
    const gfx::Point& location) {
  Send(new OzoneGpuMsg_CursorMove(widget, location));
}

}  // namespace ui
