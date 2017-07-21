// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/instrumentation/resource_coordinator/FrameResourceCoordinator.h"

#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit.mojom-blink.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {

namespace {

void onConnectionError() {}

}  // namespace

// static
bool FrameResourceCoordinator::IsEnabled() {
  return resource_coordinator::IsResourceCoordinatorEnabled();
}

// static
FrameResourceCoordinator* FrameResourceCoordinator::Create(
    service_manager::InterfaceProvider* interface_provider) {
  return new FrameResourceCoordinator(interface_provider);
}

FrameResourceCoordinator::FrameResourceCoordinator(
    service_manager::InterfaceProvider* interface_provider) {
  interface_provider->GetInterface(mojo::MakeRequest(&service_));

  service_.set_connection_error_handler(
      ConvertToBaseCallback(WTF::Bind(&onConnectionError)));
}

FrameResourceCoordinator::~FrameResourceCoordinator() = default;

void FrameResourceCoordinator::SetProperty(
    const resource_coordinator::mojom::blink::PropertyType property_type,
    const bool value) {
  service_->SetProperty(property_type, base::MakeUnique<base::Value>(value));
}

DEFINE_TRACE(FrameResourceCoordinator) {}

}  // namespace blink
