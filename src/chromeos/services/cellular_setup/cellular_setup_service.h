// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CELLULAR_SETUP_CELLULAR_SETUP_SERVICE_H_
#define CHROMEOS_SERVICES_CELLULAR_SETUP_CELLULAR_SETUP_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/services/cellular_setup/public/mojom/cellular_setup.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace chromeos {

namespace cellular_setup {

class CellularSetupBase;

// Service which provides an implementation for mojom::CellularSetup. This
// service creates one implementation and shares it among all connection
// requests.
class CellularSetupService : public service_manager::Service {
 public:
  explicit CellularSetupService(service_manager::mojom::ServiceRequest request);
  ~CellularSetupService() override;

 private:
  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  service_manager::ServiceBinding service_binding_;
  service_manager::BinderRegistry registry_;

  std::unique_ptr<CellularSetupBase> cellular_setup_;

  DISALLOW_COPY_AND_ASSIGN(CellularSetupService);
};

}  // namespace cellular_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CELLULAR_SETUP_CELLULAR_SETUP_SERVICE_H_
