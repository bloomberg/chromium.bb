// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_SERVICE_H_
#define SERVICES_VIZ_SERVICE_H_

#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/ui/gpu/interfaces/gpu_main.mojom.h"

namespace ui {
class GpuMain;
}

namespace viz {

class Service : public service_manager::Service {
 public:
  Service();
  ~Service() override;

 private:
  void BindGpuMainRequest(ui::mojom::GpuMainRequest request);

  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  service_manager::BinderRegistry registry_;

  std::unique_ptr<ui::GpuMain> gpu_main_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace viz

#endif  // SERVICES_VIZ_SERVICE_H_
