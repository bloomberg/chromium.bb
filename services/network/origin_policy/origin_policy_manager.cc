// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/origin_policy/origin_policy_manager.h"

#include <memory>
#include <utility>

namespace network {

OriginPolicyManager::OriginPolicyManager() {}

OriginPolicyManager::~OriginPolicyManager() {}

void OriginPolicyManager::AddBinding(
    mojom::OriginPolicyManagerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace network
