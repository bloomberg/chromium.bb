// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_MANAGER_H_
#define SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_MANAGER_H_

#include <string>

#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/network/public/mojom/origin_policy_manager.mojom.h"

namespace network {
class COMPONENT_EXPORT(NETWORK_SERVICE) OriginPolicyManager
    : public mojom::OriginPolicyManager {
 public:
  OriginPolicyManager();
  ~OriginPolicyManager() override;

  // Bind a request to this object.  Mojo messages
  // coming through the associated pipe will be served by this object.
  void AddBinding(mojom::OriginPolicyManagerRequest request);

  // ForTesting methods
  mojo::BindingSet<mojom::OriginPolicyManager>& GetBindingsForTesting() {
    return bindings_;
  }

 private:
  mojo::BindingSet<mojom::OriginPolicyManager> bindings_;

  DISALLOW_COPY_AND_ASSIGN(OriginPolicyManager);
};

}  // namespace network

#endif  // SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_MANAGER_H_
