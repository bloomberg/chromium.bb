// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/SensorReadingUpdater.h"

#include "core/dom/Document.h"
#include "device/generic_sensor/public/interfaces/sensor.mojom-blink.h"
#include "modules/sensor/SensorProxy.h"
#include "wtf/CurrentTime.h"

using device::mojom::blink::ReportingMode;

namespace blink {

SensorReadingUpdater::SensorReadingUpdater(SensorProxy* sensor_proxy)
    : sensor_proxy_(sensor_proxy),
      document_(sensor_proxy_->GetDocument()),
      has_pending_animation_frame_task_(false) {}

void SensorReadingUpdater::EnqueueAnimationFrameTask() {
  if (!document_ || document_->IsDetached()) {
    // If the document has detached the scheduled callbacks
    // will never be called.
    has_pending_animation_frame_task_ = false;
    document_ = sensor_proxy_->GetDocument();
    if (!document_ || document_->IsDetached())
      return;
  }

  if (has_pending_animation_frame_task_)
    return;

  auto callback = WTF::Bind(&SensorReadingUpdater::OnAnimationFrame,
                            WrapWeakPersistent(this));
  document_->EnqueueAnimationFrameTask(std::move(callback));
  has_pending_animation_frame_task_ = true;
}

void SensorReadingUpdater::Start() {
  EnqueueAnimationFrameTask();
}

void SensorReadingUpdater::OnAnimationFrame() {
  has_pending_animation_frame_task_ = false;
  OnAnimationFrameInternal();
}

DEFINE_TRACE(SensorReadingUpdater) {
  visitor->Trace(document_);
  visitor->Trace(sensor_proxy_);
}

class SensorReadingUpdaterContinuous : public SensorReadingUpdater {
 public:
  explicit SensorReadingUpdaterContinuous(SensorProxy* sensor_proxy)
      : SensorReadingUpdater(sensor_proxy) {}

  DEFINE_INLINE_VIRTUAL_TRACE() { SensorReadingUpdater::Trace(visitor); }

 protected:
  void OnAnimationFrameInternal() override {
    if (!sensor_proxy_->IsActive())
      return;

    sensor_proxy_->UpdateSensorReading();
    sensor_proxy_->NotifySensorChanged(WTF::MonotonicallyIncreasingTime());
    EnqueueAnimationFrameTask();
  }
};

// New data is fetched from shared buffer only once after 'start()'
// call. Further, notification is send until every client is updated
// (i.e. until longest notification period elapses) rAF stops after that.
class SensorReadingUpdaterOnChange : public SensorReadingUpdater {
 public:
  explicit SensorReadingUpdaterOnChange(SensorProxy* sensor_proxy)
      : SensorReadingUpdater(sensor_proxy),
        new_data_arrived_time_(0.0),
        new_data_arrived_(false) {}

  DEFINE_INLINE_VIRTUAL_TRACE() { SensorReadingUpdater::Trace(visitor); }

  void Start() override {
    new_data_arrived_ = true;
    SensorReadingUpdater::Start();
  }

 protected:
  void OnAnimationFrameInternal() override {
    if (!sensor_proxy_->IsActive())
      return;

    double timestamp = WTF::MonotonicallyIncreasingTime();

    if (new_data_arrived_) {
      new_data_arrived_ = false;
      sensor_proxy_->UpdateSensorReading();
      new_data_arrived_time_ = timestamp;
    }
    sensor_proxy_->NotifySensorChanged(timestamp);

    DCHECK_GT(sensor_proxy_->FrequenciesUsed().front(), 0.0);
    double longest_notification_period =
        1 / sensor_proxy_->FrequenciesUsed().front();

    if (timestamp - new_data_arrived_time_ <= longest_notification_period)
      EnqueueAnimationFrameTask();
  }

 private:
  double new_data_arrived_time_;
  bool new_data_arrived_;
};

// static
SensorReadingUpdater* SensorReadingUpdater::Create(SensorProxy* proxy,
                                                   ReportingMode mode) {
  if (mode == ReportingMode::CONTINUOUS)
    return new SensorReadingUpdaterContinuous(proxy);
  return new SensorReadingUpdaterOnChange(proxy);
}

}  // namespace blink
