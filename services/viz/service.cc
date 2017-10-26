// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/service.h"

#include "services/ui/gpu/gpu_main.h"
#include "services/ui/gpu/interfaces/gpu_main.mojom.h"

namespace viz {

Service::Service() = default;

Service::~Service() = default;

void Service::OnStart() {
  base::PlatformThread::SetName("VizMain");
  registry_.AddInterface<ui::mojom::GpuMain>(
      base::Bind(&Service::BindGpuMainRequest, base::Unretained(this)));

  ui::GpuMain::ExternalDependencies deps;
  deps.create_display_compositor = true;
  gpu_main_ = std::make_unique<ui::GpuMain>(nullptr, std::move(deps));
}

void Service::OnBindInterface(const service_manager::BindSourceInfo& info,
                              const std::string& interface_name,
                              mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void Service::BindGpuMainRequest(ui::mojom::GpuMainRequest request) {
  gpu_main_->Bind(std::move(request));
}

}  // namespace viz
