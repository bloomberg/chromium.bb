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

SensorReadingUpdater::SensorReadingUpdater(SensorProxy* sensorProxy)
    : m_sensorProxy(sensorProxy),
      m_document(m_sensorProxy->document()),
      m_hasPendingAnimationFrameTask(false) {}

void SensorReadingUpdater::enqueueAnimationFrameTask() {
  if (!m_document || m_document->isDetached()) {
    // If the document has detached the scheduled callbacks
    // will never be called.
    m_hasPendingAnimationFrameTask = false;
    m_document = m_sensorProxy->document();
    if (!m_document || m_document->isDetached())
      return;
  }

  if (m_hasPendingAnimationFrameTask)
    return;

  auto callback = WTF::bind(&SensorReadingUpdater::onAnimationFrame,
                            wrapWeakPersistent(this));
  m_document->enqueueAnimationFrameTask(std::move(callback));
  m_hasPendingAnimationFrameTask = true;
}

void SensorReadingUpdater::start() {
  enqueueAnimationFrameTask();
}

void SensorReadingUpdater::onAnimationFrame() {
  m_hasPendingAnimationFrameTask = false;
  onAnimationFrameInternal();
}

DEFINE_TRACE(SensorReadingUpdater) {
  visitor->trace(m_document);
  visitor->trace(m_sensorProxy);
}

class SensorReadingUpdaterContinuous : public SensorReadingUpdater {
 public:
  explicit SensorReadingUpdaterContinuous(SensorProxy* sensorProxy)
      : SensorReadingUpdater(sensorProxy) {}

  DEFINE_INLINE_VIRTUAL_TRACE() { SensorReadingUpdater::trace(visitor); }

 protected:
  void onAnimationFrameInternal() override {
    if (!m_sensorProxy->isActive())
      return;

    m_sensorProxy->updateSensorReading();
    m_sensorProxy->notifySensorChanged(WTF::monotonicallyIncreasingTime());
    enqueueAnimationFrameTask();
  }
};

// New data is fetched from shared buffer only once after 'start()'
// call. Further, notification is send until every client is updated
// (i.e. until longest notification period elapses) rAF stops after that.
class SensorReadingUpdaterOnChange : public SensorReadingUpdater {
 public:
  explicit SensorReadingUpdaterOnChange(SensorProxy* sensorProxy)
      : SensorReadingUpdater(sensorProxy),
        m_newDataArrivedTime(0.0),
        m_newDataArrived(false) {}

  DEFINE_INLINE_VIRTUAL_TRACE() { SensorReadingUpdater::trace(visitor); }

  void start() override {
    m_newDataArrived = true;
    SensorReadingUpdater::start();
  }

 protected:
  void onAnimationFrameInternal() override {
    if (!m_sensorProxy->isActive())
      return;

    double timestamp = WTF::monotonicallyIncreasingTime();

    if (m_newDataArrived) {
      m_newDataArrived = false;
      m_sensorProxy->updateSensorReading();
      m_newDataArrivedTime = timestamp;
    }
    m_sensorProxy->notifySensorChanged(timestamp);

    DCHECK_GT(m_sensorProxy->frequenciesUsed().front(), 0.0);
    double longestNotificationPeriod =
        1 / m_sensorProxy->frequenciesUsed().front();

    if (timestamp - m_newDataArrivedTime <= longestNotificationPeriod)
      enqueueAnimationFrameTask();
  }

 private:
  double m_newDataArrivedTime;
  bool m_newDataArrived;
};

// static
SensorReadingUpdater* SensorReadingUpdater::create(SensorProxy* proxy,
                                                   ReportingMode mode) {
  if (mode == ReportingMode::CONTINUOUS)
    return new SensorReadingUpdaterContinuous(proxy);
  return new SensorReadingUpdaterOnChange(proxy);
}

}  // namespace blink
