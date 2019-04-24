// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_IDENTITY_SERVICE_H_
#define SERVICES_IDENTITY_IDENTITY_SERVICE_H_

#include "components/signin/core/browser/signin_manager_base.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/mojom/identity_accessor.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"

class AccountTrackerService;

namespace identity {

class IdentityService : public service_manager::Service {
 public:
  IdentityService(IdentityManager* identity_manager,
                  AccountTrackerService* account_tracker,
                  service_manager::mojom::ServiceRequest request);
  ~IdentityService() override;

 private:
  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  void Create(mojom::IdentityAccessorRequest request);

  // Shuts down this instance, blocking it from serving any pending or future
  // requests. Safe to call multiple times; will be a no-op after the first
  // call.
  void ShutDown();
  bool IsShutDown();

  service_manager::ServiceBinding service_binding_;

  IdentityManager* identity_manager_;
  AccountTrackerService* account_tracker_;

  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(IdentityService);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_IDENTITY_SERVICE_H_
