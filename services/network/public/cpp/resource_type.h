// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_TYPE_H_
#define SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_TYPE_H_

#include <stdint.h>

namespace network {

// Network service embedders should provide a full definition for ResourceType;
// network service doesn't use this at all and just passes it through as an
// opaque value.
enum class ResourceType : int32_t;

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_TYPE_H_
