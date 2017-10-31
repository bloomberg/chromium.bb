// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FrameResourceCoordinator_h
#define FrameResourceCoordinator_h

#include "platform/instrumentation/resource_coordinator/BlinkResourceCoordinatorBase.h"

#include "platform/wtf/Noncopyable.h"

namespace service_manager {
class InterfaceProvider;
}  // namespace service_manager

namespace blink {

class PLATFORM_EXPORT FrameResourceCoordinator final
    : public BlinkResourceCoordinatorBase {
  WTF_MAKE_NONCOPYABLE(FrameResourceCoordinator);

 public:
  static FrameResourceCoordinator* Create(service_manager::InterfaceProvider*);
  ~FrameResourceCoordinator() override;

 private:
  explicit FrameResourceCoordinator(service_manager::InterfaceProvider*);
};

}  // namespace blink

#endif  // FrameResourceCoordinator_h
