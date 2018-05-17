// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_SERVICE_METRICS_H_
#define SERVICES_AUDIO_SERVICE_METRICS_H_

#include "base/time/time.h"

namespace base {
class Clock;
}

namespace audio {

class ServiceMetrics {
 public:
  explicit ServiceMetrics(base::Clock* clock);
  ~ServiceMetrics();

  void HasConnections();
  void HasNoConnections();

 private:
  void LogHasNoConnectionsDuration();

  const base::Clock* clock_;
  const base::Time service_start_;
  base::Time has_connections_start_;
  base::Time has_no_connections_start_;

  DISALLOW_COPY_AND_ASSIGN(ServiceMetrics);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_SERVICE_METRICS_H_
