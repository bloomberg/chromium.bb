// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_GPU_PLATFORM_SUPPORT_HOST_H_
#define UI_OZONE_PLATFORM_DRI_DRI_GPU_PLATFORM_SUPPORT_HOST_H_

#include <queue>
#include <vector>

#include "base/observer_list.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/dri/hardware_cursor_delegate.h"
#include "ui/ozone/public/gpu_platform_support_host.h"

class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {

class ChannelObserver;

class DriGpuPlatformSupportHost : public GpuPlatformSupportHost,
                                  public HardwareCursorDelegate,
                                  public IPC::Sender {
 public:
  DriGpuPlatformSupportHost();
  ~DriGpuPlatformSupportHost() override;

  bool IsConnected() const;

  void RegisterHandler(GpuPlatformSupportHost* handler);
  void UnregisterHandler(GpuPlatformSupportHost* handler);

  void AddChannelObserver(ChannelObserver* observer);
  void RemoveChannelObserver(ChannelObserver* observer);

  // GpuPlatformSupportHost:
  void OnChannelEstablished(int host_id, IPC::Sender* sender) override;
  void OnChannelDestroyed(int host_id) override;

  // IPC::Listener:
  bool OnMessageReceived(const IPC::Message& message) override;

  // IPC::Sender:
  bool Send(IPC::Message* message) override;

  // HardwareCursorDelegate:
  void SetHardwareCursor(gfx::AcceleratedWidget widget,
                         const std::vector<SkBitmap>& bitmaps,
                         const gfx::Point& location,
                         int frame_delay_ms) override;
  void MoveHardwareCursor(gfx::AcceleratedWidget widget,
                          const gfx::Point& location) override;

 private:
  int host_id_;
  IPC::Sender* sender_;
  std::vector<GpuPlatformSupportHost*> handlers_;
  // If messages are sent before the channel is created, store the messages and
  // delay sending them until the channel is created. These messages are stored
  // in |queued_messaged_|.
  std::queue<IPC::Message*> queued_messages_;
  ObserverList<ChannelObserver> channel_observers_;
};

}  // namespace ui

#endif  // UI_OZONE_GPU_DRI_GPU_PLATFORM_SUPPORT_HOST_H_
