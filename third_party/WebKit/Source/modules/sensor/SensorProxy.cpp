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
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"

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

void SensorProxy::AddConfiguration(SensorConfigurationPtr configuration,
                                   Function<void(bool)> callback) {
  DCHECK(IsInitialized());
  AddActiveFrequency(configuration->frequency);
  sensor_->AddConfiguration(std::move(configuration),
                            ConvertToBaseCallback(std::move(callback)));
}

void SensorProxy::RemoveConfiguration(SensorConfigurationPtr configuration) {
  DCHECK(IsInitialized());
  RemoveActiveFrequency(configuration->frequency);
  sensor_->RemoveConfiguration(std::move(configuration));
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

  if (reading_.timestamp() != reading_data.timestamp()) {
    DCHECK_GT(reading_data.timestamp(), reading_.timestamp())
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
  active_frequencies_.clear();
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
  DCHECK_GE(device::GetSensorMaxAllowedFrequency(type_),
            frequency_limits_.second);

  auto error_callback =
      WTF::Bind(&SensorProxy::HandleSensorError, WrapWeakPersistent(this));
  sensor_.set_connection_error_handler(
      ConvertToBaseCallback(std::move(error_callback)));

  state_ = kInitialized;

  UpdateSuspendedStatus();

  for (Observer* observer : observers_)
    observer->OnSensorInitialized();
}

void SensorProxy::OnPollingTimer(TimerBase*) {
  UpdateSensorReading();
}

bool SensorProxy::ShouldProcessReadings() const {
  return IsInitialized() && !suspended_ && !active_frequencies_.IsEmpty();
}

void SensorProxy::UpdatePollingStatus() {
  if (mode_ != ReportingMode::CONTINUOUS)
    return;

  if (ShouldProcessReadings()) {
    // TODO(crbug/721297) : We need to find out an algorithm for resulting
    // polling frequency.
    polling_timer_.StartRepeating(1 / active_frequencies_.back(),
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

void SensorProxy::RemoveActiveFrequency(double frequency) {
  // Can use binary search as active_frequencies_ is sorted.
  auto it = std::lower_bound(active_frequencies_.begin(),
                             active_frequencies_.end(), frequency);
  if (it == active_frequencies_.end()) {
    NOTREACHED() << "Attempted to remove active frequency which is not present "
                    "in the list";
    return;
  }

  active_frequencies_.erase(std::distance(active_frequencies_.begin(), it));
  UpdatePollingStatus();
}

void SensorProxy::AddActiveFrequency(double frequency) {
  active_frequencies_.push_back(frequency);
  std::sort(active_frequencies_.begin(), active_frequencies_.end());
  UpdatePollingStatus();
}

}  // namespace blink
