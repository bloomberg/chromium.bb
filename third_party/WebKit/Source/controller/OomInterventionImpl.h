// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OomInterventionImpl_h
#define OomInterventionImpl_h

#include "controller/ControllerExport.h"
#include "core/page/ScopedPagePauser.h"
#include "platform/Timer.h"
#include "public/platform/oom_intervention.mojom-blink.h"

namespace blink {

class OomInterventionImplTest;

// Implementation of OOM intervention. This pauses all pages by using
// ScopedPagePauser when near-OOM situation is detected.
class CONTROLLER_EXPORT OomInterventionImpl
    : public mojom::blink::OomIntervention {
 public:
  static void Create(mojom::blink::OomInterventionRequest);

  using MemoryWorkloadCaculator = base::RepeatingCallback<size_t()>;

  explicit OomInterventionImpl(MemoryWorkloadCaculator);
  ~OomInterventionImpl() override;

  // mojom::blink::OomIntervention:
  void StartDetection(mojom::blink::OomInterventionHostPtr,
                      bool trigger_intervention) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(OomInterventionImplTest, DetectedAndDeclined);

  void Check(TimerBase*);

  // This constant is declared here for testing.
  static const size_t kMemoryWorkloadThreshold;

  MemoryWorkloadCaculator workload_calculator_;
  mojom::blink::OomInterventionHostPtr host_;
  bool trigger_intervention_ = false;
  Timer<OomInterventionImpl> timer_;
  std::unique_ptr<ScopedPagePauser> pauser_;
};

}  // namespace blink

#endif  // OomInterventionImpl_h
