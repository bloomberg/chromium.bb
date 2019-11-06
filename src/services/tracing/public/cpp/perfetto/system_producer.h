// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_SYSTEM_PRODUCER_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_SYSTEM_PRODUCER_H_

#include "services/tracing/public/cpp/perfetto/perfetto_producer.h"
#include "third_party/perfetto/include/perfetto/tracing/core/producer.h"

namespace tracing {

class SystemProducer : public PerfettoProducer, public perfetto::Producer {
 public:
  SystemProducer(PerfettoTaskRunner* task_runner);
  // Since Chrome does not support concurrent tracing sessions, and system
  // tracing is always lower priority than human or DevTools initiated tracing,
  // all system producers must be able to disconnect and stop tracing.
  virtual void Disconnect() = 0;
};
}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_SYSTEM_PRODUCER_H_
