// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/SensorProviderProxy.h"

#include "modules/sensor/SensorProxy.h"
#include "platform/mojo/MojoHelper.h"
#include "services/device/public/interfaces/constants.mojom-blink.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {

// SensorProviderProxy
SensorProviderProxy::SensorProviderProxy(LocalFrame& frame)
    : Supplement<LocalFrame>(frame) {}

void SensorProviderProxy::InitializeIfNeeded() {
  if (IsInitialized())
    return;

  GetSupplementable()->GetInterfaceProvider().GetInterface(
      mojo::MakeRequest(&sensor_provider_));
  sensor_provider_.set_connection_error_handler(ConvertToBaseCallback(
      WTF::Bind(&SensorProviderProxy::OnSensorProviderConnectionError,
                WrapWeakPersistent(this))));
}

const char* SensorProviderProxy::SupplementName() {
  return "SensorProvider";
}

// static
SensorProviderProxy* SensorProviderProxy::From(LocalFrame* frame) {
  DCHECK(frame);
  SensorProviderProxy* provider_proxy = static_cast<SensorProviderProxy*>(
      Supplement<LocalFrame>::From(*frame, SupplementName()));
  if (!provider_proxy) {
    provider_proxy = new SensorProviderProxy(*frame);
    Supplement<LocalFrame>::ProvideTo(*frame, SupplementName(), provider_proxy);
  }
  provider_proxy->InitializeIfNeeded();
  return provider_proxy;
}

SensorProviderProxy::~SensorProviderProxy() {}

void SensorProviderProxy::Trace(blink::Visitor* visitor) {
  visitor->Trace(sensor_proxies_);
  Supplement<LocalFrame>::Trace(visitor);
}

SensorProxy* SensorProviderProxy::CreateSensorProxy(
    device::mojom::blink::SensorType type,
    Page* page) {
  DCHECK(!GetSensorProxy(type));

  SensorProxy* sensor = new SensorProxy(type, this, page);
  sensor_proxies_.insert(sensor);

  return sensor;
}

SensorProxy* SensorProviderProxy::GetSensorProxy(
    device::mojom::blink::SensorType type) {
  for (SensorProxy* sensor : sensor_proxies_) {
    // TODO(Mikhail) : Hash sensors by type for efficiency.
    if (sensor->type() == type)
      return sensor;
  }

  return nullptr;
}

void SensorProviderProxy::OnSensorProviderConnectionError() {
  sensor_provider_.reset();
  for (SensorProxy* sensor : sensor_proxies_)
    sensor->HandleSensorError();
}

}  // namespace blink
