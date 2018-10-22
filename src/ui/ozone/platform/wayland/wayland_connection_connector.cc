// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_connection_connector.h"

#include "base/task_runner_util.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/public/interfaces/wayland/wayland_connection.mojom.h"

namespace ui {

namespace {

// TODO(msisov): In the future when GpuProcessHost is moved to vizhost, remove
// this utility code.
using BinderCallback = ui::GpuPlatformSupportHost::GpuHostBindInterfaceCallback;

void BindInterfaceInGpuProcess(const std::string& interface_name,
                               mojo::ScopedMessagePipeHandle interface_pipe,
                               const BinderCallback& binder_callback) {
  return binder_callback.Run(interface_name, std::move(interface_pipe));
}

template <typename Interface>
void BindInterfaceInGpuProcess(mojo::InterfaceRequest<Interface> request,
                               const BinderCallback& binder_callback) {
  BindInterfaceInGpuProcess(
      Interface::Name_, std::move(request.PassMessagePipe()), binder_callback);
}

}  // namespace

WaylandConnectionConnector::WaylandConnectionConnector(
    WaylandConnection* connection)
    : connection_(connection) {}

WaylandConnectionConnector::~WaylandConnectionConnector() = default;

void WaylandConnectionConnector::OnGpuProcessLaunched(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
    scoped_refptr<base::SingleThreadTaskRunner> send_runner,
    const base::RepeatingCallback<void(IPC::Message*)>& send_callback) {}

void WaylandConnectionConnector::OnChannelDestroyed(int host_id) {
  // TODO(msisov): Handle restarting.
  NOTIMPLEMENTED();
}

void WaylandConnectionConnector::OnMessageReceived(
    const IPC::Message& message) {
  NOTREACHED() << "This class should only be used with mojo transport but here "
                  "we're wrongly getting invoked to handle IPC communication.";
}

void WaylandConnectionConnector::OnGpuServiceLaunched(
    scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner,
    GpuHostBindInterfaceCallback binder,
    GpuHostTerminateCallback terminate_callback) {
  terminate_callback_ = std::move(terminate_callback);
  binder_ = std::move(binder);

  io_runner_ = io_runner;
  auto on_terminate_gpu_cb =
      base::BindOnce(&WaylandConnectionConnector::OnTerminateGpuProcess,
                     base::Unretained(this));
  connection_->SetTerminateGpuCallback(std::move(on_terminate_gpu_cb));

  base::PostTaskAndReplyWithResult(
      ui_runner.get(), FROM_HERE,
      base::BindOnce(&WaylandConnection::BindInterface,
                     base::Unretained(connection_)),
      base::BindOnce(&WaylandConnectionConnector::OnWaylandConnectionPtrBinded,
                     base::Unretained(this)));
}

void WaylandConnectionConnector::OnWaylandConnectionPtrBinded(
    ozone::mojom::WaylandConnectionPtr wc_ptr) const {
  ozone::mojom::WaylandConnectionClientPtr wcp_ptr;
  auto request = mojo::MakeRequest(&wcp_ptr);
  BindInterfaceInGpuProcess(std::move(request), binder_);
  wcp_ptr->SetWaylandConnection(std::move(wc_ptr));
}

void WaylandConnectionConnector::OnTerminateGpuProcess(std::string message) {
  io_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(terminate_callback_),
                                                 std::move(message)));
}

}  // namespace ui
