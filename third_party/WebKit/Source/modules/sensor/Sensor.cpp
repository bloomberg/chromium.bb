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
#include "modules/sensor/SensorErrorEvent.h"
#include "modules/sensor/SensorProviderProxy.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "services/device/public/interfaces/sensor.mojom-blink.h"

namespace blink {

namespace {
const double kWaitingIntervalThreshold = 0.01;
}  // namespace

Sensor::Sensor(ExecutionContext* execution_context,
               const SensorOptions& sensor_options,
               ExceptionState& exception_state,
               device::mojom::blink::SensorType type)
    : ContextLifecycleObserver(execution_context),
      sensor_options_(sensor_options),
      type_(type),
      state_(SensorState::kIdle),
      last_reported_timestamp_(0.0) {
  // Check secure context.
  String error_message;
  if (!execution_context->IsSecureContext(error_message)) {
    exception_state.ThrowSecurityError(error_message);
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
    const double max_allowed_frequency =
        device::GetSensorMaxAllowedFrequency(type_);
    if (frequency > max_allowed_frequency) {
      sensor_options_.setFrequency(max_allowed_frequency);
      String message = String::Format(
          "Maximum allowed frequency value for this sensor type is %.0f Hz.",
          max_allowed_frequency);
      ConsoleMessage* console_message = ConsoleMessage::Create(
          kJSMessageSource, kInfoMessageLevel, std::move(message));
      execution_context->AddConsoleMessage(console_message);
    }
  }
}

Sensor::~Sensor() = default;

void Sensor::start() {
  if (state_ != SensorState::kIdle)
    return;
  state_ = SensorState::kActivating;
  Activate();
}

void Sensor::stop() {
  if (state_ == SensorState::kIdle)
    return;
  Deactivate();
  state_ = SensorState::kIdle;
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
      sensor_proxy_->reading().timestamp());
}

DEFINE_TRACE(Sensor) {
  visitor->Trace(sensor_proxy_);
  ActiveScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

bool Sensor::HasPendingActivity() const {
  if (state_ == SensorState::kIdle)
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
  if (!IsIdleOrErrored())
    Deactivate();
}

void Sensor::OnSensorInitialized() {
  if (state_ != SensorState::kActivating)
    return;

  RequestAddConfiguration();
}

void Sensor::OnSensorReadingChanged() {
  if (state_ != SensorState::kActivated)
    return;

  // Return if reading update is already scheduled or the cached
  // reading is up-to-date.
  if (pending_reading_notification_.IsActive())
    return;

  double elapsedTime =
      sensor_proxy_->reading().timestamp() - last_reported_timestamp_;
  DCHECK_GT(elapsedTime, 0.0);

  DCHECK_GT(configuration_->frequency, 0.0);
  double waitingTime = 1 / configuration_->frequency - elapsedTime;

  // Negative or zero 'waitingTime' means that polling period has elapsed.
  // We also avoid scheduling if the elapsed time is slightly behind the
  // polling period.
  auto sensor_reading_changed =
      WTF::Bind(&Sensor::NotifyReading, WrapWeakPersistent(this));
  if (waitingTime < kWaitingIntervalThreshold) {
    // Invoke JS callbacks in a different callchain to obviate
    // possible modifications of SensorProxy::observers_ container
    // while it is being iterated through.
    pending_reading_notification_ =
        TaskRunnerHelper::Get(TaskType::kSensor, GetExecutionContext())
            ->PostCancellableTask(BLINK_FROM_HERE,
                                  std::move(sensor_reading_changed));
  } else {
    pending_reading_notification_ =
        TaskRunnerHelper::Get(TaskType::kSensor, GetExecutionContext())
            ->PostDelayedCancellableTask(
                BLINK_FROM_HERE, std::move(sensor_reading_changed),
                WTF::TimeDelta::FromSecondsD(waitingTime));
  }
}

void Sensor::OnSensorError(ExceptionCode code,
                           const String& sanitized_message,
                           const String& unsanitized_message) {
  HandleError(code, sanitized_message, unsanitized_message);
}

void Sensor::OnAddConfigurationRequestCompleted(bool result) {
  if (state_ != SensorState::kActivating)
    return;

  if (!result) {
    HandleError(kNotReadableError, "start() call has failed.");
    return;
  }

  if (!GetExecutionContext())
    return;

  pending_activated_notification_ =
      TaskRunnerHelper::Get(TaskType::kSensor, GetExecutionContext())
          ->PostCancellableTask(
              BLINK_FROM_HERE,
              WTF::Bind(&Sensor::NotifyActivated, WrapWeakPersistent(this)));
}

void Sensor::Activate() {
  DCHECK_EQ(state_, SensorState::kActivating);

  InitSensorProxyIfNeeded();
  if (!sensor_proxy_) {
    HandleError(kInvalidStateError,
                "The Sensor is no longer associated to a frame.");
    return;
  }

  if (sensor_proxy_->IsInitialized())
    RequestAddConfiguration();
  else
    sensor_proxy_->Initialize();

  sensor_proxy_->AddObserver(this);
}

void Sensor::Deactivate() {
  DCHECK_NE(state_, SensorState::kIdle);
  // state_ is not set to kIdle here as on error it should
  // transition to the kIdle state in the same call chain
  // the error event is dispatched, i.e. inside NotifyError().
  pending_reading_notification_.Cancel();
  pending_activated_notification_.Cancel();
  pending_error_notification_.Cancel();

  if (!sensor_proxy_)
    return;

  if (sensor_proxy_->IsInitialized()) {
    DCHECK(configuration_);
    sensor_proxy_->RemoveConfiguration(configuration_->Clone());
    last_reported_timestamp_ = 0.0;
  }

  sensor_proxy_->RemoveObserver(this);
}

void Sensor::RequestAddConfiguration() {
  if (!configuration_) {
    configuration_ = CreateSensorConfig();
    DCHECK(configuration_);
    DCHECK_GE(configuration_->frequency,
              sensor_proxy_->FrequencyLimits().first);
    DCHECK_LE(configuration_->frequency,
              sensor_proxy_->FrequencyLimits().second);
  }

  DCHECK(sensor_proxy_);
  sensor_proxy_->AddConfiguration(
      configuration_->Clone(),
      WTF::Bind(&Sensor::OnAddConfigurationRequestCompleted,
                WrapWeakPersistent(this)));
}

void Sensor::HandleError(ExceptionCode code,
                         const String& sanitized_message,
                         const String& unsanitized_message) {
  if (!GetExecutionContext()) {
    // Deactivate() is already called from Sensor::ContextDestroyed().
    return;
  }

  if (IsIdleOrErrored())
    return;

  Deactivate();

  auto error =
      DOMException::Create(code, sanitized_message, unsanitized_message);
  pending_error_notification_ =
      TaskRunnerHelper::Get(TaskType::kSensor, GetExecutionContext())
          ->PostCancellableTask(
              BLINK_FROM_HERE,
              WTF::Bind(&Sensor::NotifyError, WrapWeakPersistent(this),
                        WrapPersistent(error)));
}

void Sensor::NotifyReading() {
  DCHECK_EQ(state_, SensorState::kActivated);
  last_reported_timestamp_ = sensor_proxy_->reading().timestamp();
  DispatchEvent(Event::Create(EventTypeNames::reading));
}

void Sensor::NotifyActivated() {
  DCHECK_EQ(state_, SensorState::kActivating);
  state_ = SensorState::kActivated;

  if (CanReturnReadings()) {
    // If reading has already arrived, send initial 'reading' notification
    // right away.
    DCHECK(!pending_reading_notification_.IsActive());
    pending_reading_notification_ =
        TaskRunnerHelper::Get(TaskType::kSensor, GetExecutionContext())
            ->PostCancellableTask(
                BLINK_FROM_HERE,
                WTF::Bind(&Sensor::NotifyReading, WrapWeakPersistent(this)));
  }

  DispatchEvent(Event::Create(EventTypeNames::activate));
}

void Sensor::NotifyError(DOMException* error) {
  DCHECK_NE(state_, SensorState::kIdle);
  state_ = SensorState::kIdle;
  DispatchEvent(SensorErrorEvent::Create(EventTypeNames::error, error));
}

bool Sensor::CanReturnReadings() const {
  if (!IsActivated())
    return false;
  DCHECK(sensor_proxy_);
  return sensor_proxy_->reading().timestamp() != 0.0;
}

bool Sensor::IsIdleOrErrored() const {
  return (state_ == SensorState::kIdle) ||
         pending_error_notification_.IsActive();
}

}  // namespace blink
