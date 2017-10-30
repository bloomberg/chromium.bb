// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlinkResourceCoordinatorBase_h
#define BlinkResourceCoordinatorBase_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Noncopyable.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"

namespace blink {

// Base class for Resource Coordinators in Blink.
class PLATFORM_EXPORT BlinkResourceCoordinatorBase {
  WTF_MAKE_NONCOPYABLE(BlinkResourceCoordinatorBase);

 public:
  static bool IsEnabled() {
    return resource_coordinator::IsResourceCoordinatorEnabled();
  }

  BlinkResourceCoordinatorBase() = default;
};

}  // namespace blink

#endif  // BlinkResourceCoordinatorBase_h
