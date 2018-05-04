// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/window_service.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "services/ui/ws2/gpu_support.h"
#include "services/ui/ws2/window_data.h"
#include "services/ui/ws2/window_service_client.h"
#include "services/ui/ws2/window_service_delegate.h"
#include "services/ui/ws2/window_tree_factory.h"

namespace ui {
namespace ws2 {

WindowService::WindowService(WindowServiceDelegate* delegate,
                             std::unique_ptr<GpuSupport> gpu_support)
    : delegate_(delegate), gpu_support_(std::move(gpu_support)) {}

WindowService::~WindowService() {}

WindowData* WindowService::GetWindowDataForWindowCreateIfNecessary(
    aura::Window* window) {
  WindowData* data = WindowData::GetMayBeNull(window);
  if (data)
    return data;

  const viz::FrameSinkId frame_sink_id =
      ClientWindowId(kWindowServerClientId, next_window_id_++);
  CHECK_NE(0u, next_window_id_);
  return WindowData::Create(window, nullptr, frame_sink_id);
}

std::unique_ptr<WindowServiceClient> WindowService::CreateWindowServiceClient(
    mojom::WindowTreeClient* window_tree_client,
    bool intercepts_events) {
  const ClientSpecificId client_id = next_client_id_++;
  CHECK_NE(0u, next_client_id_);
  return std::make_unique<WindowServiceClient>(
      this, client_id, window_tree_client, intercepts_events);
}

void WindowService::OnStart() {
  window_tree_factory_ = std::make_unique<WindowTreeFactory>(this);

  registry_.AddInterface(base::BindRepeating(
      &WindowService::BindClipboardRequest, base::Unretained(this)));
  registry_.AddInterface(base::BindRepeating(
      &WindowService::BindDisplayManagerRequest, base::Unretained(this)));
  registry_.AddInterface(base::BindRepeating(
      &WindowService::BindImeDriverRequest, base::Unretained(this)));
  registry_.AddInterface(base::BindRepeating(
      &WindowService::BindWindowTreeFactoryRequest, base::Unretained(this)));

  // |gpu_support_| may be null in tests.
  if (gpu_support_) {
    registry_.AddInterface(
        base::BindRepeating(
            &GpuSupport::BindDiscardableSharedMemoryManagerOnGpuTaskRunner,
            base::Unretained(gpu_support_.get())),
        gpu_support_->GetGpuTaskRunner());
    registry_.AddInterface(
        base::BindRepeating(&GpuSupport::BindGpuRequestOnGpuTaskRunner,
                            base::Unretained(gpu_support_.get())),
        gpu_support_->GetGpuTaskRunner());
  }
}

void WindowService::OnBindInterface(
    const service_manager::BindSourceInfo& remote_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle handle) {
  registry_.BindInterface(interface_name, std::move(handle));
}

void WindowService::BindClipboardRequest(mojom::ClipboardRequest request) {
  // TODO: https://crbug.com/839591.
  NOTIMPLEMENTED();
}

void WindowService::BindDisplayManagerRequest(
    mojom::DisplayManagerRequest request) {
  // TODO: https://crbug.com/839592.
  NOTIMPLEMENTED();
}

void WindowService::BindImeDriverRequest(mojom::IMEDriverRequest request) {
  // TODO: https://crbug.com/837710.
  NOTIMPLEMENTED();
}

void WindowService::BindWindowTreeFactoryRequest(
    ui::mojom::WindowTreeFactoryRequest request) {
  window_tree_factory_->AddBinding(std::move(request));
}

}  // namespace ws2
}  // namespace ui
