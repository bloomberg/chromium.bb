// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/Sensor.h"

#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "device/generic_sensor/public/interfaces/sensor.mojom-blink.h"
#include "modules/sensor/SensorErrorEvent.h"
#include "modules/sensor/SensorProviderProxy.h"

using namespace device::mojom::blink;

namespace blink {

Sensor::Sensor(ExecutionContext* executionContext,
               const SensorOptions& sensorOptions,
               ExceptionState& exceptionState,
               SensorType type)
    : ContextLifecycleObserver(executionContext),
      m_sensorOptions(sensorOptions),
      m_type(type),
      m_state(Sensor::SensorState::Unconnected),
      m_lastUpdateTimestamp(0.0) {
  // Check secure context.
  String errorMessage;
  if (!executionContext->isSecureContext(errorMessage)) {
    exceptionState.throwDOMException(SecurityError, errorMessage);
    return;
  }

  // Check top-level browsing context.
  if (!toDocument(executionContext)->domWindow()->frame() ||
      !toDocument(executionContext)->frame()->isMainFrame()) {
    exceptionState.throwSecurityError(
        "Must be in a top-level browsing context");
    return;
  }

  // Check the given frequency value.
  if (m_sensorOptions.hasFrequency()) {
    double frequency = m_sensorOptions.frequency();
    if (frequency > SensorConfiguration::kMaxAllowedFrequency) {
      m_sensorOptions.setFrequency(SensorConfiguration::kMaxAllowedFrequency);
      ConsoleMessage* consoleMessage = ConsoleMessage::create(
          JSMessageSource, InfoMessageLevel, "Frequency is limited to 60 Hz.");
      executionContext->addConsoleMessage(consoleMessage);
    }
  }
}

Sensor::~Sensor() = default;

void Sensor::start() {
  if (m_state != Sensor::SensorState::Unconnected &&
      m_state != Sensor::SensorState::Idle &&
      m_state != Sensor::SensorState::Errored)
    return;

  initSensorProxyIfNeeded();
  if (!m_sensorProxy) {
    reportError(InvalidStateError,
                "The Sensor is no longer associated to a frame.");
    return;
  }

  m_lastUpdateTimestamp = WTF::monotonicallyIncreasingTime();
  startListening();
}

void Sensor::stop() {
  if (m_state == Sensor::SensorState::Unconnected ||
      m_state == Sensor::SensorState::Idle ||
      m_state == Sensor::SensorState::Errored)
    return;

  stopListening();
}

static String ToString(Sensor::SensorState state) {
  switch (state) {
    case Sensor::SensorState::Unconnected:
      return "unconnected";
    case Sensor::SensorState::Activating:
      return "activating";
    case Sensor::SensorState::Activated:
      return "activated";
    case Sensor::SensorState::Idle:
      return "idle";
    case Sensor::SensorState::Errored:
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

DOMHighResTimeStamp Sensor::timestamp(ScriptState* scriptState,
                                      bool& isNull) const {
  if (!canReturnReadings()) {
    isNull = true;
    return 0.0;
  }

  LocalDOMWindow* window = scriptState->domWindow();
  if (!window) {
    isNull = true;
    return 0.0;
  }

  Performance* performance = DOMWindowPerformance::performance(*window);
  DCHECK(performance);
  DCHECK(m_sensorProxy);
  isNull = false;

  return performance->monotonicTimeToDOMHighResTimeStamp(
      m_sensorProxy->reading().timestamp);
}

DEFINE_TRACE(Sensor) {
  visitor->trace(m_sensorProxy);
  ActiveScriptWrappable::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
  EventTargetWithInlineData::trace(visitor);
}

bool Sensor::hasPendingActivity() const {
  if (m_state == Sensor::SensorState::Unconnected ||
      m_state == Sensor::SensorState::Idle ||
      m_state == Sensor::SensorState::Errored)
    return false;
  return getExecutionContext() && hasEventListeners();
}

auto Sensor::createSensorConfig() -> SensorConfigurationPtr {
  auto result = SensorConfiguration::New();

  double defaultFrequency = m_sensorProxy->defaultConfig()->frequency;
  double minimumFrequency = m_sensorProxy->frequencyLimits().first;
  double maximumFrequency = m_sensorProxy->frequencyLimits().second;

  double frequency = m_sensorOptions.hasFrequency()
                         ? m_sensorOptions.frequency()
                         : defaultFrequency;

  if (frequency > maximumFrequency)
    frequency = maximumFrequency;
  if (frequency < minimumFrequency)
    frequency = minimumFrequency;

  result->frequency = frequency;
  return result;
}

double Sensor::readingValue(int index, bool& isNull) const {
  if (!canReturnReadings()) {
    isNull = true;
    return 0.0;
  }
  DCHECK(m_sensorProxy);
  DCHECK(index >= 0 && index < device::SensorReading::kValuesCount);
  return m_sensorProxy->reading().values[index];
}

void Sensor::initSensorProxyIfNeeded() {
  if (m_sensorProxy)
    return;

  Document* document = toDocument(getExecutionContext());
  if (!document || !document->frame())
    return;

  auto provider = SensorProviderProxy::from(document->frame());
  m_sensorProxy = provider->getSensorProxy(m_type);

  if (!m_sensorProxy)
    m_sensorProxy = provider->createSensorProxy(m_type, document->page());
}

void Sensor::contextDestroyed(ExecutionContext*) {
  if (m_state == Sensor::SensorState::Activated ||
      m_state == Sensor::SensorState::Activating)
    stopListening();
}

void Sensor::onSensorInitialized() {
  if (m_state != Sensor::SensorState::Activating)
    return;

  startListening();
}

void Sensor::onSensorReadingChanged(double timestamp) {
  if (m_state != Sensor::SensorState::Activated)
    return;

  DCHECK_GT(m_configuration->frequency, 0.0);
  double period = 1 / m_configuration->frequency;
  if (timestamp - m_lastUpdateTimestamp >= period) {
    m_lastUpdateTimestamp = timestamp;
    notifySensorReadingChanged();
  }
}

void Sensor::onSensorError(ExceptionCode code,
                           const String& sanitizedMessage,
                           const String& unsanitizedMessage) {
  reportError(code, sanitizedMessage, unsanitizedMessage);
}

void Sensor::onStartRequestCompleted(bool result) {
  if (m_state != Sensor::SensorState::Activating)
    return;

  if (!result) {
    reportError(
        OperationError,
        "start() call has failed possibly due to inappropriate options.");
    return;
  }

  updateState(Sensor::SensorState::Activated);
}

void Sensor::startListening() {
  DCHECK(m_sensorProxy);
  updateState(Sensor::SensorState::Activating);

  m_sensorProxy->addObserver(this);
  if (!m_sensorProxy->isInitialized()) {
    m_sensorProxy->initialize();
    return;
  }

  if (!m_configuration) {
    m_configuration = createSensorConfig();
    DCHECK(m_configuration);
    DCHECK(m_configuration->frequency > 0 &&
           m_configuration->frequency <=
               SensorConfiguration::kMaxAllowedFrequency);
  }

  auto startCallback =
      WTF::bind(&Sensor::onStartRequestCompleted, wrapWeakPersistent(this));
  m_sensorProxy->addConfiguration(m_configuration->Clone(),
                                  std::move(startCallback));
}

void Sensor::stopListening() {
  DCHECK(m_sensorProxy);
  updateState(Sensor::SensorState::Idle);

  if (m_sensorProxy->isInitialized()) {
    DCHECK(m_configuration);
    m_sensorProxy->removeConfiguration(m_configuration->Clone());
  }
  m_sensorProxy->removeObserver(this);
}

void Sensor::updateState(Sensor::SensorState newState) {
  if (newState == m_state)
    return;

  if (newState == SensorState::Activated && getExecutionContext()) {
    DCHECK_EQ(SensorState::Activating, m_state);
    // The initial value for m_lastUpdateTimestamp is set to current time,
    // so that the first reading update will be notified considering the given
    // frequency hint.
    m_lastUpdateTimestamp = WTF::monotonicallyIncreasingTime();
    TaskRunnerHelper::get(TaskType::Sensor, getExecutionContext())
        ->postTask(BLINK_FROM_HERE, WTF::bind(&Sensor::notifyOnActivate,
                                              wrapWeakPersistent(this)));
  }

  m_state = newState;
}

void Sensor::reportError(ExceptionCode code,
                         const String& sanitizedMessage,
                         const String& unsanitizedMessage) {
  updateState(Sensor::SensorState::Errored);
  if (getExecutionContext()) {
    auto error =
        DOMException::create(code, sanitizedMessage, unsanitizedMessage);
    TaskRunnerHelper::get(TaskType::Sensor, getExecutionContext())
        ->postTask(BLINK_FROM_HERE,
                   WTF::bind(&Sensor::notifyError, wrapWeakPersistent(this),
                             wrapPersistent(error)));
  }
}

void Sensor::notifySensorReadingChanged() {
  DCHECK(m_sensorProxy);

  if (m_sensorProxy->reading().timestamp != m_storedData.timestamp) {
    m_storedData = m_sensorProxy->reading();
    dispatchEvent(Event::create(EventTypeNames::change));
  }
}

void Sensor::notifyOnActivate() {
  dispatchEvent(Event::create(EventTypeNames::activate));
}

void Sensor::notifyError(DOMException* error) {
  dispatchEvent(
      SensorErrorEvent::create(EventTypeNames::error, std::move(error)));
}

bool Sensor::canReturnReadings() const {
  if (m_state != Sensor::SensorState::Activated)
    return false;
  DCHECK(m_sensorProxy);
  return m_sensorProxy->reading().timestamp != 0.0;
}

}  // namespace blink
