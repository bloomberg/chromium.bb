// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/SensorProviderProxy.h"

#include "modules/sensor/SensorProxy.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"

namespace blink {

// SensorProviderProxy
SensorProviderProxy::SensorProviderProxy(LocalFrame& frame)
    : Supplement<LocalFrame>(frame) {}

void SensorProviderProxy::initializeIfNeeded(LocalFrame* frame) {
  if (isInitialized())
    return;

  frame->interfaceProvider()->getInterface(
      mojo::MakeRequest(&m_sensorProvider));
  m_sensorProvider.set_connection_error_handler(convertToBaseCallback(
      WTF::bind(&SensorProviderProxy::onSensorProviderConnectionError,
                wrapWeakPersistent(this))));
}

const char* SensorProviderProxy::supplementName() {
  return "SensorProvider";
}

// static
SensorProviderProxy* SensorProviderProxy::from(LocalFrame* frame) {
  DCHECK(frame);
  SensorProviderProxy* providerProxy = static_cast<SensorProviderProxy*>(
      Supplement<LocalFrame>::from(*frame, supplementName()));
  if (!providerProxy) {
    providerProxy = new SensorProviderProxy(*frame);
    Supplement<LocalFrame>::provideTo(*frame, supplementName(), providerProxy);
  }
  providerProxy->initializeIfNeeded(frame);
  return providerProxy;
}

SensorProviderProxy::~SensorProviderProxy() {}

DEFINE_TRACE(SensorProviderProxy) {
  visitor->trace(m_sensorProxies);
  Supplement<LocalFrame>::trace(visitor);
}

SensorProxy* SensorProviderProxy::createSensorProxy(
    device::mojom::blink::SensorType type,
    Page* page) {
  DCHECK(!getSensorProxy(type));

  SensorProxy* sensor = new SensorProxy(type, this, page);
  m_sensorProxies.insert(sensor);

  return sensor;
}

SensorProxy* SensorProviderProxy::getSensorProxy(
    device::mojom::blink::SensorType type) {
  for (SensorProxy* sensor : m_sensorProxies) {
    // TODO(Mikhail) : Hash sensors by type for efficiency.
    if (sensor->type() == type)
      return sensor;
  }

  return nullptr;
}

void SensorProviderProxy::onSensorProviderConnectionError() {
  m_sensorProvider.reset();
  for (SensorProxy* sensor : m_sensorProxies)
    sensor->handleSensorError();
}

}  // namespace blink
