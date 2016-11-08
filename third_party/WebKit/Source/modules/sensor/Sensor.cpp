// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/Sensor.h"

#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/inspector/ConsoleMessage.h"
#include "device/generic_sensor/public/interfaces/sensor.mojom-blink.h"
#include "modules/sensor/SensorErrorEvent.h"
#include "modules/sensor/SensorPollingStrategy.h"
#include "modules/sensor/SensorProviderProxy.h"
#include "modules/sensor/SensorReading.h"

using namespace device::mojom::blink;

namespace blink {

Sensor::Sensor(ScriptState* scriptState,
               const SensorOptions& sensorOptions,
               ExceptionState& exceptionState,
               SensorType type)
    : ActiveScriptWrappable(this),
      ContextLifecycleObserver(scriptState->getExecutionContext()),
      PageVisibilityObserver(
          toDocument(scriptState->getExecutionContext())->page()),
      m_sensorOptions(sensorOptions),
      m_type(type),
      m_state(Sensor::SensorState::IDLE) {
  // Check secure context.
  String errorMessage;
  if (!scriptState->getExecutionContext()->isSecureContext(errorMessage)) {
    exceptionState.throwDOMException(SecurityError, errorMessage);
    return;
  }

  // Check top-level browsing context.
  if (!scriptState->domWindow() || !scriptState->domWindow()->frame() ||
      !scriptState->domWindow()->frame()->isMainFrame()) {
    exceptionState.throwSecurityError(
        "Must be in a top-level browsing context");
    return;
  }

  // Check the given frequency value.
  if (m_sensorOptions.hasFrequency()) {
    double frequency = m_sensorOptions.frequency();
    if (frequency <= 0.0) {
      exceptionState.throwRangeError("Frequency must be positive.");
      return;
    }

    if (frequency > SensorConfiguration::kMaxAllowedFrequency) {
      m_sensorOptions.setFrequency(SensorConfiguration::kMaxAllowedFrequency);
      ConsoleMessage* consoleMessage = ConsoleMessage::create(
          JSMessageSource, InfoMessageLevel, "Frequency is limited to 60 Hz.");
      scriptState->getExecutionContext()->addConsoleMessage(consoleMessage);
    }
  }
}

Sensor::~Sensor() = default;

void Sensor::start(ScriptState* scriptState, ExceptionState& exceptionState) {
  if (m_state != Sensor::SensorState::IDLE &&
      m_state != Sensor::SensorState::ERRORED) {
    exceptionState.throwDOMException(
        InvalidStateError,
        "Cannot start because SensorState is not idle or errored");
    return;
  }

  initSensorProxyIfNeeded();

  if (!m_sensorProxy) {
    exceptionState.throwDOMException(
        InvalidStateError, "The Sensor is no longer associated to a frame.");
    return;
  }

  startListening();
}

void Sensor::stop(ScriptState*, ExceptionState& exceptionState) {
  if (m_state == Sensor::SensorState::IDLE ||
      m_state == Sensor::SensorState::ERRORED) {
    exceptionState.throwDOMException(
        InvalidStateError,
        "Cannot stop because SensorState is either idle or errored");
    return;
  }

  stopListening();
}

static String ToString(Sensor::SensorState state) {
  switch (state) {
    case Sensor::SensorState::IDLE:
      return "idle";
    case Sensor::SensorState::ACTIVATING:
      return "activating";
    case Sensor::SensorState::ACTIVE:
      return "active";
    case Sensor::SensorState::ERRORED:
      return "errored";
    default:
      NOTREACHED();
  }
  return "idle";
}

// Getters
String Sensor::state() const {
  return ToString(m_state);
}

SensorReading* Sensor::reading() const {
  if (m_state != Sensor::SensorState::ACTIVE)
    return nullptr;
  DCHECK(m_sensorProxy);
  return m_sensorProxy->sensorReading();
}

DEFINE_TRACE(Sensor) {
  visitor->trace(m_sensorProxy);
  ActiveScriptWrappable::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
  PageVisibilityObserver::trace(visitor);
  EventTargetWithInlineData::trace(visitor);
}

bool Sensor::hasPendingActivity() const {
  if (m_state == Sensor::SensorState::IDLE ||
      m_state == Sensor::SensorState::ERRORED)
    return false;
  return hasEventListeners();
}

void Sensor::initSensorProxyIfNeeded() {
  if (m_sensorProxy)
    return;

  Document* document = toDocument(getExecutionContext());
  if (!document || !document->frame())
    return;

  auto provider = SensorProviderProxy::from(document->frame());
  m_sensorProxy = provider->getSensor(m_type);

  if (!m_sensorProxy) {
    m_sensorProxy =
        provider->createSensor(m_type, createSensorReadingFactory());
  }
}

void Sensor::contextDestroyed() {
  if (m_state == Sensor::SensorState::ACTIVE ||
      m_state == Sensor::SensorState::ACTIVATING)
    stopListening();
}

void Sensor::onSensorInitialized() {
  if (m_state != Sensor::SensorState::ACTIVATING)
    return;

  startListening();
}

void Sensor::onSensorReadingChanged() {
  if (m_polling)
    m_polling->onSensorReadingChanged();
}

void Sensor::onSensorError(ExceptionCode code,
                           const String& sanitizedMessage,
                           const String& unsanitizedMessage) {
  reportError(code, sanitizedMessage, unsanitizedMessage);
}

void Sensor::onStartRequestCompleted(bool result) {
  if (m_state != Sensor::SensorState::ACTIVATING)
    return;

  if (!result) {
    reportError(
        OperationError,
        "start() call has failed possibly due to inappropriate options.");
    return;
  }

  DCHECK(m_configuration);
  DCHECK(m_sensorProxy);
  auto pollCallback = WTF::bind(&Sensor::pollForData, wrapWeakPersistent(this));
  DCHECK_GT(m_configuration->frequency, 0);
  m_polling = SensorPollingStrategy::create(1 / m_configuration->frequency,
                                            std::move(pollCallback),
                                            m_sensorProxy->reportingMode());
  updateState(Sensor::SensorState::ACTIVE);
}

void Sensor::onStopRequestCompleted(bool result) {
  if (m_state == Sensor::SensorState::IDLE)
    return;

  if (!result)
    reportError(OperationError);

  DCHECK(m_sensorProxy);
  m_sensorProxy->removeObserver(this);
}

void Sensor::pageVisibilityChanged() {
  updatePollingStatus();

  if (!m_sensorProxy || !m_sensorProxy->isInitialized())
    return;

  if (page()->visibilityState() != PageVisibilityStateVisible) {
    m_sensorProxy->suspend();
  } else {
    m_sensorProxy->resume();
  }
}

void Sensor::startListening() {
  DCHECK(m_sensorProxy);
  updateState(Sensor::SensorState::ACTIVATING);

  m_sensorProxy->addObserver(this);
  if (!m_sensorProxy->isInitialized()) {
    m_sensorProxy->initialize();
    return;
  }

  if (!m_configuration) {
    m_configuration =
        createSensorConfig(m_sensorOptions, *m_sensorProxy->defaultConfig());
    DCHECK(m_configuration);
  }

  auto startCallback =
      WTF::bind(&Sensor::onStartRequestCompleted, wrapWeakPersistent(this));
  m_sensorProxy->addConfiguration(m_configuration->Clone(),
                                  std::move(startCallback));
}

void Sensor::stopListening() {
  DCHECK(m_sensorProxy);
  updateState(Sensor::SensorState::IDLE);

  if (m_sensorProxy->isInitialized()) {
    auto callback =
        WTF::bind(&Sensor::onStopRequestCompleted, wrapWeakPersistent(this));
    DCHECK(m_configuration);
    m_sensorProxy->removeConfiguration(m_configuration->Clone(),
                                       std::move(callback));
  } else {
    m_sensorProxy->removeObserver(this);
  }
}

void Sensor::pollForData() {
  if (m_state != Sensor::SensorState::ACTIVE) {
    DCHECK(m_polling);
    m_polling->stopPolling();
    return;
  }

  DCHECK(m_sensorProxy);
  DCHECK(m_sensorProxy->isInitialized());
  m_sensorProxy->updateSensorReading();

  DCHECK(m_sensorProxy->sensorReading());
  if (getExecutionContext() &&
      m_sensorProxy->sensorReading()->isReadingUpdated(m_storedData)) {
    getExecutionContext()->postTask(
        BLINK_FROM_HERE,
        createSameThreadTask(&Sensor::notifySensorReadingChanged,
                             wrapWeakPersistent(this)));
  }

  m_storedData = m_sensorProxy->sensorReading()->data();
}

void Sensor::updateState(Sensor::SensorState newState) {
  if (newState == m_state)
    return;
  m_state = newState;
  if (getExecutionContext()) {
    getExecutionContext()->postTask(
        BLINK_FROM_HERE, createSameThreadTask(&Sensor::notifyStateChanged,
                                              wrapWeakPersistent(this)));
  }

  updatePollingStatus();
}

void Sensor::reportError(ExceptionCode code,
                         const String& sanitizedMessage,
                         const String& unsanitizedMessage) {
  updateState(Sensor::SensorState::ERRORED);
  if (getExecutionContext()) {
    auto error =
        DOMException::create(code, sanitizedMessage, unsanitizedMessage);
    getExecutionContext()->postTask(
        BLINK_FROM_HERE,
        createSameThreadTask(&Sensor::notifyError, wrapWeakPersistent(this),
                             wrapPersistent(error)));
  }
}

void Sensor::updatePollingStatus() {
  if (!m_polling)
    return;

  if (m_state != Sensor::SensorState::ACTIVE ||
      page()->visibilityState() != PageVisibilityStateVisible) {
    m_polling->stopPolling();
  } else {
    m_polling->startPolling();
  }
}

void Sensor::notifySensorReadingChanged() {
  dispatchEvent(Event::create(EventTypeNames::change));
}

void Sensor::notifyStateChanged() {
  dispatchEvent(Event::create(EventTypeNames::statechange));
}

void Sensor::notifyError(DOMException* error) {
  dispatchEvent(
      SensorErrorEvent::create(EventTypeNames::error, std::move(error)));
}

}  // namespace blink
