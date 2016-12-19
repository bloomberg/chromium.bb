// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SensorUpdateNotificationStrategy_h
#define SensorUpdateNotificationStrategy_h

#include "wtf/Functional.h"

namespace blink {

// This class encapsulates sensor reading update notification logic:
// the callback is invoked after client calls 'onSensorReadingChanged()'
// however considering the given sample frequency:
// guaranteed not to be called more often than expected.
class SensorUpdateNotificationStrategy {
 public:
  static std::unique_ptr<SensorUpdateNotificationStrategy> create(
      double frequency,
      std::unique_ptr<Function<void()>>);

  virtual void onSensorReadingChanged() = 0;
  virtual void cancelPendingNotifications() = 0;
  virtual ~SensorUpdateNotificationStrategy() {}
};

}  // namespace blink

#endif  // SensorUpdateNotificationStrategy_h
