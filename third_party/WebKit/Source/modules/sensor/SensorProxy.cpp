// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/SensorProxy.h"

#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/LocalFrame.h"
#include "core/page/FocusController.h"
#include "modules/sensor/SensorProviderProxy.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/Platform.h"

namespace blink {

using namespace device::mojom::blink;

SensorProxy::SensorProxy(SensorType sensor_type,
                         SensorProviderProxy* provider,
                         Page* page)
    : PageVisibilityObserver(page),
      FocusChangedObserver(page),
      type_(sensor_type),
      mode_(ReportingMode::CONTINUOUS),
      provider_(provider),
      client_binding_(this),
      state_(SensorProxy::kUninitialized),
      suspended_(false),
      polling_timer_(TaskRunnerHelper::Get(TaskType::kSensor,
                                           provider->GetSupplementable()),
                     this,
                     &SensorProxy::OnPollingTimer) {}

SensorProxy::~SensorProxy() {}

void SensorProxy::Dispose() {
  client_binding_.Close();
}

DEFINE_TRACE(SensorProxy) {
  visitor->Trace(observers_);
  visitor->Trace(provider_);
  PageVisibilityObserver::Trace(visitor);
}

void SensorProxy::AddObserver(Observer* observer) {
  if (!observers_.Contains(observer))
    observers_.insert(observer);
}

void SensorProxy::RemoveObserver(Observer* observer) {
  observers_.erase(observer);
}

void SensorProxy::Initialize() {
  if (state_ != kUninitialized)
    return;

  if (!provider_->GetSensorProvider()) {
    HandleSensorError();
    return;
  }

  state_ = kInitializing;
  auto callback = ConvertToBaseCallback(
      WTF::Bind(&SensorProxy::OnSensorCreated, WrapWeakPersistent(this)));
  provider_->GetSensorProvider()->GetSensor(type_, mojo::MakeRequest(&sensor_),
                                            callback);
}

void SensorProxy::AddConfiguration(
    SensorConfigurationPtr configuration,
    std::unique_ptr<Function<void(bool)>> callback) {
  DCHECK(IsInitialized());
  auto wrapper = WTF::Bind(&SensorProxy::OnAddConfigurationCompleted,
                           WrapWeakPersistent(this), configuration->frequency,
                           WTF::Passed(std::move(callback)));
  sensor_->AddConfiguration(std::move(configuration),
                            ConvertToBaseCallback(std::move(wrapper)));
}

void SensorProxy::RemoveConfiguration(SensorConfigurationPtr configuration) {
  DCHECK(IsInitialized());
  auto callback = WTF::Bind(&SensorProxy::OnRemoveConfigurationCompleted,
                            WrapWeakPersistent(this), configuration->frequency);
  sensor_->RemoveConfiguration(std::move(configuration),
                               ConvertToBaseCallback(std::move(callback)));
}

void SensorProxy::Suspend() {
  DCHECK(IsInitialized());
  if (suspended_)
    return;

  sensor_->Suspend();
  suspended_ = true;
  UpdatePollingStatus();
}

void SensorProxy::Resume() {
  DCHECK(IsInitialized());
  if (!suspended_)
    return;

  sensor_->Resume();
  suspended_ = false;
  UpdatePollingStatus();
}

const SensorConfiguration* SensorProxy::DefaultConfig() const {
  DCHECK(IsInitialized());
  return default_config_.get();
}

void SensorProxy::UpdateSensorReading() {
  DCHECK(ShouldProcessReadings());
  device::SensorReading reading_data;
  if (!shared_buffer_handle_->is_valid() ||
      !shared_buffer_reader_->GetReading(&reading_data)) {
    HandleSensorError();
    return;
  }

  if (reading_.timestamp != reading_data.timestamp) {
    DCHECK_GT(reading_data.timestamp, reading_.timestamp)
        << "Timestamps must increase monotonically";
    reading_ = reading_data;
    for (Observer* observer : observers_)
      observer->OnSensorReadingChanged();
  }
}

void SensorProxy::RaiseError() {
  HandleSensorError();
}

void SensorProxy::SensorReadingChanged() {
  DCHECK_EQ(ReportingMode::ON_CHANGE, mode_);
  if (ShouldProcessReadings())
    UpdateSensorReading();
}

void SensorProxy::PageVisibilityChanged() {
  UpdateSuspendedStatus();
}

void SensorProxy::FocusedFrameChanged() {
  UpdateSuspendedStatus();
}

void SensorProxy::HandleSensorError() {
  state_ = kUninitialized;
  frequencies_used_.clear();
  reading_ = device::SensorReading();
  UpdatePollingStatus();

  // The m_sensor.reset() will release all callbacks and its bound parameters,
  // therefore, handleSensorError accepts messages by value.
  sensor_.reset();
  shared_buffer_.reset();
  shared_buffer_handle_.reset();
  default_config_.reset();
  client_binding_.Close();

  auto copy = observers_;
  for (Observer* observer : copy) {
    observer->OnSensorError(kNotReadableError, "Could not connect to a sensor",
                            String());
  }
}

void SensorProxy::OnSensorCreated(SensorInitParamsPtr params,
                                  SensorClientRequest client_request) {
  DCHECK_EQ(kInitializing, state_);
  if (!params) {
    HandleSensorError();
    return;
  }
  const size_t kReadBufferSize = sizeof(ReadingBuffer);

  DCHECK_EQ(0u, params->buffer_offset % kReadBufferSize);

  mode_ = params->mode;
  default_config_ = std::move(params->default_configuration);
  if (!default_config_) {
    HandleSensorError();
    return;
  }

  DCHECK(sensor_.is_bound());
  client_binding_.Bind(std::move(client_request));

  shared_buffer_handle_ = std::move(params->memory);
  DCHECK(!shared_buffer_);
  shared_buffer_ = shared_buffer_handle_->MapAtOffset(kReadBufferSize,
                                                      params->buffer_offset);

  if (!shared_buffer_) {
    HandleSensorError();
    return;
  }

  const auto* buffer = static_cast<const device::SensorReadingSharedBuffer*>(
      shared_buffer_.get());
  shared_buffer_reader_.reset(
      new device::SensorReadingSharedBufferReader(buffer));
  frequency_limits_.first = params->minimum_frequency;
  frequency_limits_.second = params->maximum_frequency;

  DCHECK_GT(frequency_limits_.first, 0.0);
  DCHECK_GE(frequency_limits_.second, frequency_limits_.first);
  constexpr double kMaxAllowedFrequency =
      SensorConfiguration::kMaxAllowedFrequency;
  DCHECK_GE(kMaxAllowedFrequency, frequency_limits_.second);

  auto error_callback =
      WTF::Bind(&SensorProxy::HandleSensorError, WrapWeakPersistent(this));
  sensor_.set_connection_error_handler(
      ConvertToBaseCallback(std::move(error_callback)));

  state_ = kInitialized;

  UpdateSuspendedStatus();

  for (Observer* observer : observers_)
    observer->OnSensorInitialized();
}

void SensorProxy::OnAddConfigurationCompleted(
    double frequency,
    std::unique_ptr<Function<void(bool)>> callback,
    bool result) {
  if (result) {
    frequencies_used_.push_back(frequency);
    std::sort(frequencies_used_.begin(), frequencies_used_.end());
    UpdatePollingStatus();
  }

  (*callback)(result);
}

void SensorProxy::OnRemoveConfigurationCompleted(double frequency,
                                                 bool result) {
  if (!result) {
    DVLOG(1) << "Failure at sensor configuration removal";
    return;
  }

  size_t index = frequencies_used_.Find(frequency);
  if (index == kNotFound) {
    // Could happen e.g. if 'handleSensorError' was called before.
    return;
  }

  frequencies_used_.erase(index);
  UpdatePollingStatus();
}

void SensorProxy::OnPollingTimer(TimerBase*) {
  UpdateSensorReading();
}

bool SensorProxy::ShouldProcessReadings() const {
  return IsInitialized() && !suspended_ && !frequencies_used_.IsEmpty();
}

void SensorProxy::UpdatePollingStatus() {
  if (mode_ != ReportingMode::CONTINUOUS)
    return;

  if (ShouldProcessReadings()) {
    // TODO(crbug/721297) : We need to find out an algorithm for resulting
    // polling frequency.
    polling_timer_.StartRepeating(1 / frequencies_used_.back(),
                                  BLINK_FROM_HERE);
  } else {
    polling_timer_.Stop();
  }
}

void SensorProxy::UpdateSuspendedStatus() {
  if (!IsInitialized())
    return;

  bool page_visible =
      GetPage()->VisibilityState() == kPageVisibilityStateVisible;

  LocalFrame* focused_frame = GetPage()->GetFocusController().FocusedFrame();
  bool main_frame_focused =
      focused_frame && !focused_frame->IsCrossOriginSubframe();

  if (page_visible && main_frame_focused)
    Resume();
  else
    Suspend();
}

}  // namespace blink
