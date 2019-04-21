// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "components/viz/service/main/viz_main_impl.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "media/gpu/buildflags.h"
#include "services/viz/privileged/interfaces/viz_main.mojom.h"

#if defined(OS_CHROMEOS) && BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif

namespace viz {

Service::Service(service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)) {}

Service::~Service() = default;

void Service::OnStart() {
  base::PlatformThread::SetName("VizMain");
  registry_.AddInterface<mojom::VizMain>(
      base::Bind(&Service::BindVizMainRequest, base::Unretained(this)));

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  gpu::GpuPreferences gpu_preferences;
  if (command_line->HasSwitch(switches::kGpuPreferences)) {
    std::string value =
        command_line->GetSwitchValueASCII(switches::kGpuPreferences);
    bool success = gpu_preferences.FromSwitchValue(value);
    CHECK(success);
  }

  auto gpu_init = std::make_unique<gpu::GpuInit>();
  gpu_init->set_sandbox_helper(this);
  gpu_init->InitializeAndStartSandbox(command_line, gpu_preferences);

  VizMainImpl::ExternalDependencies deps;
  deps.create_display_compositor = true;
  deps.connector = service_binding_.GetConnector();
  viz_main_ = std::make_unique<VizMainImpl>(nullptr, std::move(deps),
                                            std::move(gpu_init));
}

void Service::OnBindInterface(const service_manager::BindSourceInfo& info,
                              const std::string& interface_name,
                              mojo::ScopedMessagePipeHandle interface_pipe) {
  if (registry_.TryBindInterface(interface_name, &interface_pipe))
    return;
#if defined(USE_OZONE)
  viz_main_->BindInterface(interface_name, std::move(interface_pipe));
#else
  NOTREACHED();
#endif
}

void Service::BindVizMainRequest(mojom::VizMainRequest request) {
  viz_main_->Bind(std::move(request));
}

void Service::PreSandboxStartup() {
#if defined(OS_CHROMEOS) && BUILDFLAG(USE_VAAPI)
  media::VaapiWrapper::PreSandboxInitialization();
#endif
  // TODO(sad): https://crbug.com/645602
}

bool Service::EnsureSandboxInitialized(gpu::GpuWatchdogThread* watchdog_thread,
                                       const gpu::GPUInfo* gpu_info,
                                       const gpu::GpuPreferences& gpu_prefs) {
  // TODO(sad): https://crbug.com/645602
  return true;
}

}  // namespace viz
