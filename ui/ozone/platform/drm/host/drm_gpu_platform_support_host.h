// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_GPU_PLATFORM_SUPPORT_HOST_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_GPU_PLATFORM_SUPPORT_HOST_H_

#include <vector>

#include "base/callback.h"
#include "base/observer_list.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/gpu_platform_support_host.h"

class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {

class DrmCursor;
class GpuThreadObserver;

class DrmGpuPlatformSupportHost : public GpuPlatformSupportHost,
                                  public IPC::Sender {
 public:
  DrmGpuPlatformSupportHost(DrmCursor* cursor);
  ~DrmGpuPlatformSupportHost() override;

  void RegisterHandler(GpuPlatformSupportHost* handler);
  void UnregisterHandler(GpuPlatformSupportHost* handler);

  void AddGpuThreadObserver(GpuThreadObserver* observer);
  void RemoveGpuThreadObserver(GpuThreadObserver* observer);

  bool IsConnected();

  // GpuPlatformSupportHost:
  void OnChannelEstablished(
      int host_id,
      scoped_refptr<base::SingleThreadTaskRunner> send_runner,
      const base::Callback<void(IPC::Message*)>& send_callback) override;
  void OnChannelDestroyed(int host_id) override;

  // IPC::Listener:
  bool OnMessageReceived(const IPC::Message& message) override;

  // IPC::Sender:
  bool Send(IPC::Message* message) override;

 private:
  int host_id_ = -1;

  scoped_refptr<base::SingleThreadTaskRunner> send_runner_;
  base::Callback<void(IPC::Message*)> send_callback_;

  std::vector<GpuPlatformSupportHost*> handlers_;  // Not owned.
  DrmCursor* cursor_;                              // Not owned.
  base::ObserverList<GpuThreadObserver> gpu_thread_observers_;
};

}  // namespace ui

#endif  // UI_OZONE_GPU_DRM_GPU_PLATFORM_SUPPORT_HOST_H_
