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

Sensor::Sensor(ExecutionContext* execution_context,
               const SensorOptions& sensor_options,
               ExceptionState& exception_state,
               SensorType type)
    : ContextLifecycleObserver(execution_context),
      sensor_options_(sensor_options),
      type_(type),
      state_(SensorState::kIdle),
      last_update_timestamp_(0.0) {
  // Check secure context.
  String error_message;
  if (!execution_context->IsSecureContext(error_message)) {
    exception_state.ThrowDOMException(kSecurityError, error_message);
    return;
  }

  // Check top-level browsing context.
  if (!ToDocument(execution_context)->domWindow()->GetFrame() ||
      !ToDocument(execution_context)->GetFrame()->IsMainFrame()) {
    exception_state.ThrowSecurityError(
        "Must be in a top-level browsing context");
    return;
  }

  // Check the given frequency value.
  if (sensor_options_.hasFrequency()) {
    double frequency = sensor_options_.frequency();
    if (frequency > SensorConfiguration::kMaxAllowedFrequency) {
      sensor_options_.setFrequency(SensorConfiguration::kMaxAllowedFrequency);
      ConsoleMessage* console_message =
          ConsoleMessage::Create(kJSMessageSource, kInfoMessageLevel,
                                 "Frequency is limited to 60 Hz.");
      execution_context->AddConsoleMessage(console_message);
    }
  }
}

Sensor::~Sensor() = default;

void Sensor::start() {
  if (state_ != Sensor::SensorState::kIdle)
    return;

  InitSensorProxyIfNeeded();
  if (!sensor_proxy_) {
    ReportError(kInvalidStateError,
                "The Sensor is no longer associated to a frame.");
    return;
  }

  last_update_timestamp_ = WTF::MonotonicallyIncreasingTime();
  StartListening();
}

void Sensor::stop() {
  if (state_ == Sensor::SensorState::kIdle)
    return;

  StopListening();
}

// Getters
bool Sensor::activated() const {
  return state_ == SensorState::kActivated;
}

DOMHighResTimeStamp Sensor::timestamp(ScriptState* script_state,
                                      bool& is_null) const {
  if (!CanReturnReadings()) {
    is_null = true;
    return 0.0;
  }

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!window) {
    is_null = true;
    return 0.0;
  }

  Performance* performance = DOMWindowPerformance::performance(*window);
  DCHECK(performance);
  DCHECK(sensor_proxy_);
  is_null = false;

  return performance->MonotonicTimeToDOMHighResTimeStamp(
      sensor_proxy_->Reading().timestamp);
}

DEFINE_TRACE(Sensor) {
  visitor->Trace(sensor_proxy_);
  ActiveScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

bool Sensor::HasPendingActivity() const {
  if (state_ == Sensor::SensorState::kIdle)
    return false;
  return GetExecutionContext() && HasEventListeners();
}

auto Sensor::CreateSensorConfig() -> SensorConfigurationPtr {
  auto result = SensorConfiguration::New();

  double default_frequency = sensor_proxy_->DefaultConfig()->frequency;
  double minimum_frequency = sensor_proxy_->FrequencyLimits().first;
  double maximum_frequency = sensor_proxy_->FrequencyLimits().second;

  double frequency = sensor_options_.hasFrequency()
                         ? sensor_options_.frequency()
                         : default_frequency;

  if (frequency > maximum_frequency)
    frequency = maximum_frequency;
  if (frequency < minimum_frequency)
    frequency = minimum_frequency;

  result->frequency = frequency;
  return result;
}

double Sensor::ReadingValue(int index, bool& is_null) const {
  is_null = !CanReturnReadings();
  return is_null ? 0.0 : ReadingValueUnchecked(index);
}

double Sensor::ReadingValueUnchecked(int index) const {
  DCHECK(sensor_proxy_);
  DCHECK(index >= 0 && index < device::SensorReading::kValuesCount);
  return sensor_proxy_->Reading().values[index];
}

void Sensor::InitSensorProxyIfNeeded() {
  if (sensor_proxy_)
    return;

  Document* document = ToDocument(GetExecutionContext());
  if (!document || !document->GetFrame())
    return;

  auto provider = SensorProviderProxy::From(document->GetFrame());
  sensor_proxy_ = provider->GetSensorProxy(type_);

  if (!sensor_proxy_)
    sensor_proxy_ = provider->CreateSensorProxy(type_, document->GetPage());
}

void Sensor::ContextDestroyed(ExecutionContext*) {
  if (state_ == Sensor::SensorState::kActivated ||
      state_ == Sensor::SensorState::kActivating)
    StopListening();
}

void Sensor::OnSensorInitialized() {
  if (state_ != Sensor::SensorState::kActivating)
    return;

  StartListening();
}

void Sensor::NotifySensorChanged(double timestamp) {
  if (state_ != Sensor::SensorState::kActivated)
    return;

  DCHECK_GT(configuration_->frequency, 0.0);
  double period = 1 / configuration_->frequency;

  if (timestamp - last_update_timestamp_ >= period) {
    last_update_timestamp_ = timestamp;
    NotifySensorReadingChanged();
  }
}

void Sensor::OnSensorError(ExceptionCode code,
                           const String& sanitized_message,
                           const String& unsanitized_message) {
  ReportError(code, sanitized_message, unsanitized_message);
}

void Sensor::OnStartRequestCompleted(bool result) {
  if (state_ != SensorState::kActivating)
    return;

  if (!result) {
    ReportError(kNotReadableError, "start() call has failed.");
    return;
  }

  UpdateState(Sensor::SensorState::kActivated);
}

void Sensor::StartListening() {
  DCHECK(sensor_proxy_);
  UpdateState(Sensor::SensorState::kActivating);

  sensor_proxy_->AddObserver(this);
  if (!sensor_proxy_->IsInitialized()) {
    sensor_proxy_->Initialize();
    return;
  }

  if (!configuration_) {
    configuration_ = CreateSensorConfig();
    DCHECK(configuration_);
    DCHECK(configuration_->frequency > 0 &&
           configuration_->frequency <=
               SensorConfiguration::kMaxAllowedFrequency);
  }

  auto start_callback =
      WTF::Bind(&Sensor::OnStartRequestCompleted, WrapWeakPersistent(this));
  sensor_proxy_->AddConfiguration(configuration_->Clone(),
                                  std::move(start_callback));
}

void Sensor::StopListening() {
  DCHECK(sensor_proxy_);
  UpdateState(Sensor::SensorState::kIdle);

  if (sensor_proxy_->IsInitialized()) {
    DCHECK(configuration_);
    sensor_proxy_->RemoveConfiguration(configuration_->Clone());
  }
  sensor_proxy_->RemoveObserver(this);
}

void Sensor::UpdateState(Sensor::SensorState new_state) {
  if (new_state == state_)
    return;

  if (new_state == SensorState::kActivated && GetExecutionContext()) {
    DCHECK_EQ(SensorState::kActivating, state_);
    // The initial value for m_lastUpdateTimestamp is set to current time,
    // so that the first reading update will be notified considering the given
    // frequency hint.
    last_update_timestamp_ = WTF::MonotonicallyIncreasingTime();
    TaskRunnerHelper::Get(TaskType::kSensor, GetExecutionContext())
        ->PostTask(BLINK_FROM_HERE, WTF::Bind(&Sensor::NotifyOnActivate,
                                              WrapWeakPersistent(this)));
  }

  state_ = new_state;
}

void Sensor::ReportError(ExceptionCode code,
                         const String& sanitized_message,
                         const String& unsanitized_message) {
  UpdateState(SensorState::kIdle);
  if (GetExecutionContext()) {
    auto error =
        DOMException::Create(code, sanitized_message, unsanitized_message);
    TaskRunnerHelper::Get(TaskType::kSensor, GetExecutionContext())
        ->PostTask(BLINK_FROM_HERE,
                   WTF::Bind(&Sensor::NotifyError, WrapWeakPersistent(this),
                             WrapPersistent(error)));
  }
}

void Sensor::NotifySensorReadingChanged() {
  DCHECK(sensor_proxy_);

  if (sensor_proxy_->Reading().timestamp != stored_data_.timestamp) {
    stored_data_ = sensor_proxy_->Reading();
    DispatchEvent(Event::Create(EventTypeNames::change));
  }
}

void Sensor::NotifyOnActivate() {
  DispatchEvent(Event::Create(EventTypeNames::activate));
}

void Sensor::NotifyError(DOMException* error) {
  DispatchEvent(
      SensorErrorEvent::Create(EventTypeNames::error, std::move(error)));
}

bool Sensor::CanReturnReadings() const {
  if (!IsActivated())
    return false;
  DCHECK(sensor_proxy_);
  return sensor_proxy_->Reading().timestamp != 0.0;
}

}  // namespace blink
