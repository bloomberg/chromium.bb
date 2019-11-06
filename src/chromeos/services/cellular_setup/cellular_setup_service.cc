// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/cellular_setup_service.h"

#include "base/bind.h"
#include "chromeos/services/cellular_setup/cellular_setup_base.h"
#include "chromeos/services/cellular_setup/cellular_setup_impl.h"

namespace chromeos {

namespace cellular_setup {

CellularSetupService::CellularSetupService(
    service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)),
      cellular_setup_(CellularSetupImpl::Factory::Create()) {}

CellularSetupService::~CellularSetupService() = default;

void CellularSetupService::OnStart() {
  registry_.AddInterface(
      base::BindRepeating(&CellularSetupBase::BindRequest,
                          base::Unretained(cellular_setup_.get())));
}

void CellularSetupService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace cellular_setup

}  // namespace chromeos
