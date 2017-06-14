// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/instrumentation/resource_coordinator/FrameResourceCoordinator.h"

#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit_provider.mojom-blink.h"

namespace blink {

namespace {

void onConnectionError() {}

}  // namespace

// static
bool FrameResourceCoordinator::IsEnabled() {
  return base::FeatureList::IsEnabled(features::kGlobalResourceCoordinator);
}

// static
FrameResourceCoordinator* FrameResourceCoordinator::Create(
    InterfaceProvider* interface_provider) {
  return new FrameResourceCoordinator(interface_provider);
}

FrameResourceCoordinator::FrameResourceCoordinator(
    InterfaceProvider* interface_provider) {
  interface_provider->GetInterface(mojo::MakeRequest(&service_));

  service_.set_connection_error_handler(
      ConvertToBaseCallback(WTF::Bind(&onConnectionError)));

  resource_coordinator::mojom::blink::EventPtr event =
      resource_coordinator::mojom::blink::Event::New();
  event->type = resource_coordinator::mojom::EventType::kOnRendererFrameCreated;
  service_->SendEvent(std::move(event));
}

FrameResourceCoordinator::~FrameResourceCoordinator() = default;

void FrameResourceCoordinator::SendEvent(
    const resource_coordinator::mojom::blink::EventType& event_type) {
  resource_coordinator::mojom::blink::EventPtr event =
      resource_coordinator::mojom::blink::Event::New();
  event->type = event_type;

  service_->SendEvent(std::move(event));
}

DEFINE_TRACE(FrameResourceCoordinator) {}

}  // namespace blink
