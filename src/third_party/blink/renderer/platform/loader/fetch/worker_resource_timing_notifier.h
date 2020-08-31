// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_WORKER_RESOURCE_TIMING_NOTIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_WORKER_RESOURCE_TIMING_NOTIFIER_H_

#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/timing/worker_timing_container.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

// This class is used by WorkerFetchContext to add a resource timing to an
// appropriate Performance Timeline.
// https://w3c.github.io/performance-timeline/#performance-timeline
class WorkerResourceTimingNotifier
    : public GarbageCollected<WorkerResourceTimingNotifier> {
 public:
  WorkerResourceTimingNotifier() = default;
  virtual ~WorkerResourceTimingNotifier() = default;

  // Adds a resource timing info to the associated performance timeline.
  // Implementations should route the info to an appropriate Performance
  // Timeline which may be associated with a different thread from the current
  // running thread.
  virtual void AddResourceTiming(
      mojom::blink::ResourceTimingInfoPtr,
      const AtomicString& initiator_type,
      mojo::PendingReceiver<mojom::blink::WorkerTimingContainer>
          worker_timing_receiver) = 0;

  virtual void Trace(Visitor*) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_WORKER_RESOURCE_TIMING_NOTIFIER_H_
