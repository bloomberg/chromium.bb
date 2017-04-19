// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/SensorProxy.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "modules/sensor/SensorProviderProxy.h"
#include "modules/sensor/SensorReadingUpdater.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/Platform.h"

using namespace device::mojom::blink;

namespace blink {

SensorProxy::SensorProxy(SensorType sensor_type,
                         SensorProviderProxy* provider,
                         Page* page)
    : PageVisibilityObserver(page),
      type_(sensor_type),
      mode_(ReportingMode::CONTINUOUS),
      provider_(provider),
      client_binding_(this),
      state_(SensorProxy::kUninitialized),
      suspended_(false) {}

SensorProxy::~SensorProxy() {}

void SensorProxy::Dispose() {
  client_binding_.Close();
}

DEFINE_TRACE(SensorProxy) {
  visitor->Trace(reading_updater_);
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

bool SensorProxy::IsActive() const {
  return IsInitialized() && !suspended_ && !frequencies_used_.IsEmpty();
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
}

void SensorProxy::Resume() {
  DCHECK(IsInitialized());
  if (!suspended_)
    return;

  sensor_->Resume();
  suspended_ = false;

  if (IsActive())
    reading_updater_->Start();
}

const SensorConfiguration* SensorProxy::DefaultConfig() const {
  DCHECK(IsInitialized());
  return default_config_.get();
}

Document* SensorProxy::GetDocument() const {
  return provider_->GetSupplementable()->GetDocument();
}

void SensorProxy::UpdateSensorReading() {
  DCHECK(IsInitialized());
  int read_attempts = 0;
  const int kMaxReadAttemptsCount = 10;
  device::SensorReading reading_data;
  while (!TryReadFromBuffer(reading_data)) {
    if (++read_attempts == kMaxReadAttemptsCount) {
      HandleSensorError();
      return;
    }
  }

  if (reading_.timestamp != reading_data.timestamp) {
    reading_ = reading_data;
    for (Observer* observer : observers_)
      observer->OnSensorReadingChanged();
  }
}

void SensorProxy::NotifySensorChanged(double timestamp) {
  // This notification leads to sync 'onchange' event sending, so
  // we must cache m_observers as it can be modified within event handlers.
  auto copy = observers_;
  for (Observer* observer : copy)
    observer->NotifySensorChanged(timestamp);
}

void SensorProxy::RaiseError() {
  HandleSensorError();
}

void SensorProxy::SensorReadingChanged() {
  DCHECK_EQ(ReportingMode::ON_CHANGE, mode_);
  if (IsActive())
    reading_updater_->Start();
}

void SensorProxy::PageVisibilityChanged() {
  if (!IsInitialized())
    return;

  if (GetPage()->VisibilityState() != kPageVisibilityStateVisible) {
    Suspend();
  } else {
    Resume();
  }
}

void SensorProxy::HandleSensorError() {
  state_ = kUninitialized;
  frequencies_used_.clear();
  reading_ = device::SensorReading();

  // The m_sensor.reset() will release all callbacks and its bound parameters,
  // therefore, handleSensorError accepts messages by value.
  sensor_.reset();
  shared_buffer_.reset();
  shared_buffer_handle_.reset();
  default_config_.reset();
  client_binding_.Close();

  for (Observer* observer : observers_) {
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

  reading_updater_ = SensorReadingUpdater::Create(this, mode_);

  state_ = kInitialized;

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
    if (IsActive())
      reading_updater_->Start();
  }

  (*callback)(result);
}

void SensorProxy::OnRemoveConfigurationCompleted(double frequency,
                                                 bool result) {
  if (!result)
    DVLOG(1) << "Failure at sensor configuration removal";

  size_t index = frequencies_used_.Find(frequency);
  if (index == kNotFound) {
    // Could happen e.g. if 'handleSensorError' was called before.
    return;
  }

  frequencies_used_.erase(index);
}

bool SensorProxy::TryReadFromBuffer(device::SensorReading& result) {
  DCHECK(IsInitialized());
  const ReadingBuffer* buffer =
      static_cast<const ReadingBuffer*>(shared_buffer_.get());
  const device::OneWriterSeqLock& seqlock = buffer->seqlock.value();
  auto version = seqlock.ReadBegin();
  auto reading_data = buffer->reading;
  if (seqlock.ReadRetry(version))
    return false;
  result = reading_data;
  return true;
}

}  // namespace blink
