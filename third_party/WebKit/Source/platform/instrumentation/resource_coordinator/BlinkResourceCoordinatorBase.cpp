// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/instrumentation/resource_coordinator/BlinkResourceCoordinatorBase.h"

#include "platform/wtf/Functional.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit_provider.mojom-blink.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {

// static
bool BlinkResourceCoordinatorBase::IsEnabled() {
  return resource_coordinator::IsResourceCoordinatorEnabled();
}

BlinkResourceCoordinatorBase::~BlinkResourceCoordinatorBase() = default;

BlinkResourceCoordinatorBase::BlinkResourceCoordinatorBase(
    service_manager::InterfaceProvider* interface_provider) {
  interface_provider->GetInterface(mojo::MakeRequest(&service_));
}

BlinkResourceCoordinatorBase::BlinkResourceCoordinatorBase(
    service_manager::Connector* connector,
    const std::string& service_name) {
  connector->BindInterface(service_name, &service_);
}

BlinkResourceCoordinatorBase::BlinkResourceCoordinatorBase() {}

void BlinkResourceCoordinatorBase::SetProperty(
    const resource_coordinator::mojom::blink::PropertyType property_type,
    int64_t value) {
  // service_ can be uninitialized in testing.
  if (service_)
    service_->SetProperty(property_type, value);
}

}  // namespace blink
