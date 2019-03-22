// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_CONNECTION_CONNECTOR_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_CONNECTION_CONNECTOR_H_

#include "ui/ozone/public/gpu_platform_support_host.h"

#include "ui/ozone/public/interfaces/wayland/wayland_connection.mojom.h"

namespace ui {

class WaylandConnection;

// A connector class, which instantiates a connection between
// WaylandConnectionProxy on the GPU side and the WaylandConnection object on
// the browser process side.
class WaylandConnectionConnector : public GpuPlatformSupportHost {
 public:
  WaylandConnectionConnector(WaylandConnection* connection);
  ~WaylandConnectionConnector() override;

  // GpuPlatformSupportHost:
  void OnGpuProcessLaunched(
      int host_id,
      scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
      scoped_refptr<base::SingleThreadTaskRunner> send_runner,
      const base::RepeatingCallback<void(IPC::Message*)>& send_callback)
      override;
  void OnChannelDestroyed(int host_id) override;
  void OnMessageReceived(const IPC::Message& message) override;
  void OnGpuServiceLaunched(
      scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_runner,
      GpuHostBindInterfaceCallback binder,
      GpuHostTerminateCallback terminate_callback) override;

 private:
  void OnWaylandConnectionPtrBinded(
      ozone::mojom::WaylandConnectionPtr wc_ptr) const;

  void OnTerminateGpuProcess(std::string message);

  // Non-owning pointer, which is used to bind a mojo pointer to the
  // WaylandConnection.
  WaylandConnection* connection_ = nullptr;

  GpuHostBindInterfaceCallback binder_;
  GpuHostTerminateCallback terminate_callback_;

  scoped_refptr<base::SingleThreadTaskRunner> io_runner_;

  DISALLOW_COPY_AND_ASSIGN(WaylandConnectionConnector);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_CONNECTION_CONNECTOR_H_
