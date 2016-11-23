// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/SensorUpdateNotificationStrategy.h"

#include "device/generic_sensor/public/interfaces/sensor.mojom-blink.h"
#include "platform/Timer.h"
#include "wtf/CurrentTime.h"

namespace blink {

// Polls the buffer on signal from platform but not more frequently
// than the given 'pollingPeriod'.
class SensorUpdateNotificationStrategyImpl
    : public SensorUpdateNotificationStrategy {
 public:
  SensorUpdateNotificationStrategyImpl(double frequency,
                                       std::unique_ptr<Function<void()>> func)
      : m_pollingPeriod(1 / frequency),
        m_func(std::move(func)),
        m_timer(this, &SensorUpdateNotificationStrategyImpl::onTimer),
        m_lastPollingTimestamp(0.0) {
    DCHECK_GT(frequency, 0.0);
    DCHECK(m_func);
  }

 private:
  // SensorUpdateNotificationStrategy overrides.
  void onSensorReadingChanged() override;
  void cancelPendingNotifications() override;

  void onTimer(TimerBase*);
  void notifyUpdate();

  double m_pollingPeriod;
  std::unique_ptr<Function<void()>> m_func;
  Timer<SensorUpdateNotificationStrategyImpl> m_timer;
  double m_lastPollingTimestamp;
};

void SensorUpdateNotificationStrategyImpl::onSensorReadingChanged() {
  if (m_timer.isActive())
    return;  // Skipping changes if update notification was already sheduled.

  double elapsedTime =
      WTF::monotonicallyIncreasingTime() - m_lastPollingTimestamp;

  double waitingTime = m_pollingPeriod - elapsedTime;
  const double minInterval =
      1 / device::mojom::blink::SensorConfiguration::kMaxAllowedFrequency;

  // Negative or zero 'waitingTime' means that polling period has elapsed.
  // We also avoid scheduling if the elapsed time is slightly behind the
  // polling period.
  if (waitingTime < minInterval) {
    notifyUpdate();
  } else {
    m_timer.startOneShot(waitingTime, BLINK_FROM_HERE);
  }
}

void SensorUpdateNotificationStrategyImpl::cancelPendingNotifications() {
  m_timer.stop();
}

void SensorUpdateNotificationStrategyImpl::notifyUpdate() {
  m_lastPollingTimestamp = WTF::monotonicallyIncreasingTime();
  (*m_func)();
}

void SensorUpdateNotificationStrategyImpl::onTimer(TimerBase*) {
  notifyUpdate();
}

// static
std::unique_ptr<SensorUpdateNotificationStrategy>
SensorUpdateNotificationStrategy::create(
    double pollingPeriod,
    std::unique_ptr<Function<void()>> func) {
  return std::unique_ptr<SensorUpdateNotificationStrategy>(
      new SensorUpdateNotificationStrategyImpl(pollingPeriod, std::move(func)));
}

}  // namespace blink
