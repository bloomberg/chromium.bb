// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/mirroring_service.h"

#include "base/callback.h"
#include "components/mirroring/service/session.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "services/ws/public/cpp/gpu/gpu.h"
#include "ui/base/ui_base_features.h"

namespace mirroring {

MirroringService::MirroringService(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : io_task_runner_(std::move(io_task_runner)) {
  registry_.AddInterface<mojom::MirroringService>(
      base::BindRepeating(&MirroringService::Create, base::Unretained(this)));
}

MirroringService::~MirroringService() {
  session_.reset();
  registry_.RemoveInterface<mojom::MirroringService>();
}

void MirroringService::OnStart() {
  ref_factory_.reset(new service_manager::ServiceContextRefFactory(
      context()->CreateQuitClosure()));
}

void MirroringService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

bool MirroringService::OnServiceManagerConnectionLost() {
  bindings_.CloseAllBindings();
  return true;
}

void MirroringService::Create(mojom::MirroringServiceRequest request) {
  bindings_.AddBinding(this, std::move(request));
  bindings_.set_connection_error_handler(base::BindRepeating(
      [](MirroringService* service) {
        service->session_.reset();
        service->bindings_.CloseAllBindings();
      },
      this));
}

void MirroringService::Start(mojom::SessionParametersPtr params,
                             const gfx::Size& max_resolution,
                             mojom::SessionObserverPtr observer,
                             mojom::ResourceProviderPtr resource_provider,
                             mojom::CastMessageChannelPtr outbound_channel,
                             mojom::CastMessageChannelRequest inbound_channel) {
  session_.reset();  // Stops the current session if active.
  std::unique_ptr<ws::Gpu> gpu = nullptr;
  if (params->type != mojom::SessionType::AUDIO_ONLY) {
    gpu = ws::Gpu::Create(
        context()->connector(),
        features::IsUsingWindowService() ? "ui" : "content_browser",
        io_task_runner_);
  }
  session_ = std::make_unique<Session>(
      std::move(params), max_resolution, std::move(observer),
      std::move(resource_provider), std::move(outbound_channel),
      std::move(inbound_channel), std::move(gpu));
}

}  // namespace mirroring
