// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_ipp_validator/cups_ipp_validator_service.h"

#include "base/logging.h"
#include "chrome/services/cups_ipp_validator/ipp_validator.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace {

void OnIppValidatorRequest(
    service_manager::ServiceContextRefFactory* ref_factory,
    chrome::mojom::IppValidatorRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<chrome::IppValidator>(ref_factory->CreateRef()),
      std::move(request));
}

}  // namespace

CupsIppValidatorService::CupsIppValidatorService() = default;

CupsIppValidatorService::~CupsIppValidatorService() = default;

std::unique_ptr<service_manager::Service>
CupsIppValidatorService::CreateService() {
  return std::make_unique<CupsIppValidatorService>();
}

void CupsIppValidatorService::OnStart() {
  ref_factory_ = std::make_unique<service_manager::ServiceContextRefFactory>(
      context()->CreateQuitClosure());
  registry_.AddInterface(
      base::BindRepeating(&OnIppValidatorRequest, ref_factory_.get()));

  DVLOG(1) << "CupsIppValidatorService started.";
}

void CupsIppValidatorService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}
