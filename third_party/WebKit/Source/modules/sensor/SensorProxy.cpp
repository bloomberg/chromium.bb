// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/SensorProxy.h"

#include "core/frame/LocalFrame.h"
#include "modules/sensor/SensorProviderProxy.h"
#include "platform/mojo/MojoHelper.h"

using namespace device::mojom::blink;

namespace blink {

SensorProxy::SensorProxy(SensorType sensorType, SensorProviderProxy* provider)
    : m_type(sensorType)
    , m_mode(ReportingMode::CONTINUOUS)
    , m_provider(provider)
    , m_clientBinding(this)
    , m_state(SensorProxy::Uninitialized)
    , m_reading()
{
}

SensorProxy::~SensorProxy()
{
}

void SensorProxy::dispose()
{
    m_clientBinding.Close();
}

DEFINE_TRACE(SensorProxy)
{
    visitor->trace(m_observers);
    visitor->trace(m_provider);
}

void SensorProxy::addObserver(Observer* observer)
{
    if (!m_observers.contains(observer))
        m_observers.add(observer);
}

void SensorProxy::removeObserver(Observer* observer)
{
    m_observers.remove(observer);
}

void SensorProxy::initialize()
{
    if (m_state != Uninitialized)
        return;

    if (!m_provider->sensorProvider()) {
        handleSensorError();
        return;
    }

    m_state = Initializing;
    auto callback = convertToBaseCallback(WTF::bind(&SensorProxy::onSensorCreated, wrapWeakPersistent(this)));
    m_provider->sensorProvider()->GetSensor(m_type, mojo::GetProxy(&m_sensor), callback);
    m_sensor.set_connection_error_handler(convertToBaseCallback(WTF::bind(&SensorProxy::handleSensorError, wrapWeakPersistent(this))));
}

void SensorProxy::addConfiguration(SensorConfigurationPtr configuration, std::unique_ptr<Function<void(bool)>> callback)
{
    DCHECK(isInitialized());
    m_sensor->AddConfiguration(std::move(configuration), convertToBaseCallback(std::move(callback)));
}

void SensorProxy::removeConfiguration(SensorConfigurationPtr configuration, std::unique_ptr<Function<void(bool)>> callback)
{
    DCHECK(isInitialized());
    m_sensor->RemoveConfiguration(std::move(configuration), convertToBaseCallback(std::move(callback)));
}

void SensorProxy::updateInternalReading()
{
    DCHECK(isInitialized());
    Reading* reading = static_cast<Reading*>(m_sharedBuffer.get());
    m_reading = *reading;
}

void SensorProxy::RaiseError()
{
    handleSensorError();
}

void SensorProxy::SensorReadingChanged()
{
    for (Observer* observer : m_observers)
        observer->onSensorReadingChanged();
}

void SensorProxy::handleSensorError()
{
    m_state = Uninitialized;
    m_sensor.reset();
    for (Observer* observer : m_observers)
        observer->onSensorError();
}

void SensorProxy::onSensorCreated(SensorReadBufferPtr buffer, SensorClientRequest clientRequest)
{
    DCHECK_EQ(Initializing, m_state);
    if (!buffer) {
        handleSensorError();
        return;
    }

    DCHECK_EQ(0u, buffer->offset % SensorReadBuffer::kReadBufferSize);

    m_mode = buffer->mode;

    DCHECK(m_sensor.is_bound());
    m_clientBinding.Bind(std::move(clientRequest));

    m_sharedBufferHandle = std::move(buffer->memory);
    DCHECK(!m_sharedBuffer);
    m_sharedBuffer = m_sharedBufferHandle->MapAtOffset(buffer->offset, SensorReadBuffer::kReadBufferSize);

    if (!m_sharedBuffer) {
        handleSensorError();
        return;
    }

    m_state = Initialized;
    for (Observer* observer : m_observers)
        observer->onSensorInitialized();
}

} // namespace blink
