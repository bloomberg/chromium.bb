// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_IDENTITY_SERVICE_H_
#define SERVICES_IDENTITY_IDENTITY_SERVICE_H_

#include "services/identity/public/interfaces/identity_manager.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service.h"

class SigninManagerBase;

namespace identity {

class IdentityService
    : public service_manager::Service,
      public service_manager::InterfaceFactory<mojom::IdentityManager> {
 public:
  IdentityService(SigninManagerBase* signin_manager);
  ~IdentityService() override;

 private:
  // |Service| override:
  void OnStart() override;
  void OnBindInterface(const service_manager::ServiceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // InterfaceFactory<mojom::IdentityManager>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::IdentityManagerRequest request) override;

  SigninManagerBase* signin_manager_;

  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(IdentityService);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_IDENTITY_SERVICE_H_
