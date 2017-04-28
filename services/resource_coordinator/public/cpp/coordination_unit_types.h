// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_TYPES_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_TYPES_H_

#include <stdint.h>

namespace resource_coordinator {

// Any new type here needs to be mirrored between coordination_unit_types.h and
// coordination_unit.mojom, and have mappings between the two defined in
// coordination_unit_struct_traits.h/.cc
enum class CoordinationUnitType : uint8_t {
  kInvalidType,
  kWebContents,
  kFrame,
  kNavigation,
  kProcess,
};

}  // resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_TYPES_H_
