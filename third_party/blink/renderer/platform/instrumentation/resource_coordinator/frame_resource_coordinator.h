// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_RESOURCE_COORDINATOR_FRAME_RESOURCE_COORDINATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_RESOURCE_COORDINATOR_FRAME_RESOURCE_COORDINATOR_H_

#include <memory>

#include "base/macros.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom-blink.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace service_manager {
class InterfaceProvider;
}  // namespace service_manager

namespace blink {

class PLATFORM_EXPORT FrameResourceCoordinator final {
  USING_FAST_MALLOC(FrameResourceCoordinator);

 public:
  // Returns nullptr if instrumentation is not enabled.
  static std::unique_ptr<FrameResourceCoordinator> MaybeCreate(
      service_manager::InterfaceProvider*);
  ~FrameResourceCoordinator();

  void SetNetworkAlmostIdle(bool);
  void SetLifecycleState(resource_coordinator::mojom::LifecycleState);
  void SetHasNonEmptyBeforeUnload(bool has_nonempty_beforeunload);
  void SetInterventionPolicy(
      resource_coordinator::mojom::PolicyControlledIntervention intervention,
      resource_coordinator::mojom::InterventionPolicy policy);
  void OnNonPersistentNotificationCreated();

 private:
  explicit FrameResourceCoordinator(service_manager::InterfaceProvider*);

  resource_coordinator::mojom::blink::FrameCoordinationUnitPtr service_;

  DISALLOW_COPY_AND_ASSIGN(FrameResourceCoordinator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_RESOURCE_COORDINATOR_FRAME_RESOURCE_COORDINATOR_H_
