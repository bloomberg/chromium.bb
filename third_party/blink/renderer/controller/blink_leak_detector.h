// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_BLINK_LEAK_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_BLINK_LEAK_DETECTOR_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/mojom/leak_detector/leak_detector.mojom-blink.h"
#include "third_party/blink/renderer/controller/controller_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class ResourceFetcher;

// Implementation of Leak Detector.
class CONTROLLER_EXPORT BlinkLeakDetector : public mojom::blink::LeakDetector {
 public:
  static BlinkLeakDetector& Instance();
  static void Bind(mojom::blink::LeakDetectorRequest);
  ~BlinkLeakDetector() override;

  // This registration function needs to be called whenever ResourceFetcher is
  // created (i.e. ResourceFetcher::Create is called).
  void RegisterResourceFetcher(ResourceFetcher*);

 private:
  BlinkLeakDetector();

  // mojom::blink::LeakDetector implementation.
  void PerformLeakDetection(PerformLeakDetectionCallback) override;

  void TimerFiredGC(TimerBase*);
  void ReportResult();

  TaskRunnerTimer<BlinkLeakDetector> delayed_gc_timer_;
  int number_of_gc_needed_ = 0;
  PersistentHeapHashSet<WeakMember<ResourceFetcher>> resource_fetchers_;
  PerformLeakDetectionCallback callback_;
  mojo::Binding<mojom::blink::LeakDetector> binding_;

  DISALLOW_COPY_AND_ASSIGN(BlinkLeakDetector);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CONTROLLER_BLINK_LEAK_DETECTOR_H_
