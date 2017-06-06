// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FrameResourceCoordinator_h
#define FrameResourceCoordinator_h

#include "platform/heap/Handle.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit.mojom-blink.h"

namespace blink {

class InterfaceProvider;

class PLATFORM_EXPORT FrameResourceCoordinator final
    : public GarbageCollectedFinalized<FrameResourceCoordinator> {
  WTF_MAKE_NONCOPYABLE(FrameResourceCoordinator);

 public:
  static bool IsEnabled();
  static FrameResourceCoordinator* Create(InterfaceProvider*);
  virtual ~FrameResourceCoordinator();

  DECLARE_TRACE();

 private:
  explicit FrameResourceCoordinator(InterfaceProvider*);

  resource_coordinator::mojom::blink::CoordinationUnitPtr service_;
};

}  // namespace blink

#endif  // FrameResourceCoordinator_h
