// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/sensor_proxy.h"

#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_screen_info.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/modules/sensor/sensor_provider_proxy.h"
#include "third_party/blink/renderer/modules/sensor/sensor_reading_remapper.h"
#include "third_party/blink/renderer/platform/layout_test_support.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"

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
      polling_timer_(
          provider->GetSupplementable()->GetTaskRunner(TaskType::kSensor),
          this,
          &SensorProxy::OnPollingTimer) {}

SensorProxy::~SensorProxy() = default;

void SensorProxy::Dispose() {
  client_binding_.Close();
}

void SensorProxy::Trace(blink::Visitor* visitor) {
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
  auto callback =
      WTF::Bind(&SensorProxy::OnSensorCreated, WrapWeakPersistent(this));
  provider_->GetSensorProvider()->GetSensor(type_, std::move(callback));
}

void SensorProxy::AddConfiguration(SensorConfigurationPtr configuration,
                                   base::OnceCallback<void(bool)> callback) {
  DCHECK(IsInitialized());
  AddActiveFrequency(configuration->frequency);
  sensor_->AddConfiguration(std::move(configuration), std::move(callback));
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

const device::SensorReading& SensorProxy::GetReading(bool remapped) const {
  if (remapped) {
    if (remapped_reading_.timestamp() != reading_.timestamp()) {
      remapped_reading_ = reading_;
      SensorReadingRemapper::RemapToScreenCoords(
          type_, GetScreenOrientationAngle(), &remapped_reading_);
    }
    return remapped_reading_;
  }
  return reading_;
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

  double latest_timestamp = reading_data.timestamp();
  if (reading_.timestamp() != latest_timestamp &&
      latest_timestamp != 0.0)  // The shared buffer is zeroed when
                                // sensor is stopped, we skip this
                                // reading.
  {
    DCHECK_GT(latest_timestamp, reading_.timestamp())
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

void SensorProxy::HandleSensorError(SensorCreationResult error) {
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

  ExceptionCode code = kNotReadableError;
  String description = "Could not connect to a sensor";
  if (error == SensorCreationResult::ERROR_NOT_ALLOWED) {
    code = kNotAllowedError;
    description = "Permissions to access sensor are not granted";
  }
  auto copy = observers_;
  for (Observer* observer : copy) {
    observer->OnSensorError(code, description, String());
  }
}

void SensorProxy::OnSensorCreated(SensorCreationResult result,
                                  SensorInitParamsPtr params) {
  DCHECK_EQ(kInitializing, state_);
  if (!params) {
    DCHECK_NE(SensorCreationResult::SUCCESS, result);
    HandleSensorError(result);
    return;
  }

  DCHECK_EQ(SensorCreationResult::SUCCESS, result);
  const size_t kReadBufferSize = sizeof(ReadingBuffer);

  DCHECK_EQ(0u, params->buffer_offset % kReadBufferSize);

  mode_ = params->mode;
  default_config_ = std::move(params->default_configuration);
  if (!default_config_) {
    HandleSensorError();
    return;
  }

  sensor_.Bind(std::move(params->sensor));
  client_binding_.Bind(std::move(params->client_request));

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
      WTF::Bind(&SensorProxy::HandleSensorError, WrapWeakPersistent(this),
                SensorCreationResult::ERROR_NOT_AVAILABLE);
  sensor_.set_connection_error_handler(std::move(error_callback));

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

uint16_t SensorProxy::GetScreenOrientationAngle() const {
  DCHECK(IsInitialized());
  if (LayoutTestSupport::IsRunningLayoutTest()) {
    // Simulate that the device is turned 90 degrees on the right.
    // 'orientation_angle' must be 270 as per
    // https://w3c.github.io/screen-orientation/#dfn-update-the-orientation-information.
    return 270;
  }
  return GetPage()->GetChromeClient().GetScreenInfo().orientation_angle;
}

void SensorProxy::UpdatePollingStatus() {
  if (mode_ != ReportingMode::CONTINUOUS)
    return;

  if (ShouldProcessReadings()) {
    // TODO(crbug/721297) : We need to find out an algorithm for resulting
    // polling frequency.
    polling_timer_.StartRepeating(
        WTF::TimeDelta::FromSecondsD(1 / active_frequencies_.back()),
        FROM_HERE);
  } else {
    polling_timer_.Stop();
  }
}

void SensorProxy::UpdateSuspendedStatus() {
  if (!IsInitialized())
    return;

  bool page_visible =
      GetPage()->VisibilityState() == mojom::PageVisibilityState::kVisible;

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

  active_frequencies_.erase(it);
  UpdatePollingStatus();

  if (active_frequencies_.IsEmpty())
    reading_ = device::SensorReading();
}

void SensorProxy::AddActiveFrequency(double frequency) {
  active_frequencies_.push_back(frequency);
  std::sort(active_frequencies_.begin(), active_frequencies_.end());
  UpdatePollingStatus();
}

}  // namespace blink
