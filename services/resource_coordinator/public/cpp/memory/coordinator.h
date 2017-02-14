// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEMORY_INSTRUMENTATION_PUBLIC_CPP_COORDINATOR_H_
#define SERVICES_MEMORY_INSTRUMENTATION_PUBLIC_CPP_COORDINATOR_H_

#include "services/resource_coordinator/public/interfaces/memory/memory_instrumentation.mojom.h"

namespace memory_instrumentation {

class Coordinator {
 public:
  // Binds a CoordinatorRequest to this Coordinator instance.
  virtual void BindCoordinatorRequest(mojom::CoordinatorRequest) = 0;
};

}  // namespace memory_instrumentation

#endif  // SERVICES_MEMORY_INSTRUMENTATION_PUBLIC_CPP_COORDINATOR_H_
