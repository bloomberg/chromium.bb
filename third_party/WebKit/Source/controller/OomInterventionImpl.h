// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_OOM_INTERVENTION_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_OOM_INTERVENTION_IMPL_H_

#include "third_party/blink/public/platform/oom_intervention.mojom-blink.h"
#include "third_party/blink/renderer/controller/controller_export.h"
#include "third_party/blink/renderer/core/page/scoped_page_pauser.h"
#include "third_party/blink/renderer/platform/timer.h"

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
  TaskRunnerTimer<OomInterventionImpl> timer_;
  std::unique_ptr<ScopedPagePauser> pauser_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CONTROLLER_OOM_INTERVENTION_IMPL_H_
