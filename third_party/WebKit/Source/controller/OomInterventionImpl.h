// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OomInterventionImpl_h
#define OomInterventionImpl_h

#include "controller/ControllerExport.h"
#include "public/platform/oom_intervention.mojom-blink.h"

namespace blink {

class ScopedPagePauser;

// Implementation of OOM intervention. This pauses all pages by using
// ScopedPagePauser when near-OOM situation is detected.
class CONTROLLER_EXPORT OomInterventionImpl
    : public mojom::blink::OomIntervention {
 public:
  static void Create(mojom::blink::OomInterventionRequest);

  OomInterventionImpl();
  ~OomInterventionImpl() override;

  // mojom::OomIntervention implementations:
  void OnNearOomDetected(OnNearOomDetectedCallback) override;
  void OnInterventionDeclined(OnInterventionDeclinedCallback) override;

 private:
  std::unique_ptr<ScopedPagePauser> pauser_;
};

}  // namespace blink

#endif  // OomInterventionImpl_h
