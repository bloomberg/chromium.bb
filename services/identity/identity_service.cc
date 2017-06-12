// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/identity_service.h"

#include "services/identity/identity_manager.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace identity {

IdentityService::IdentityService(SigninManagerBase* signin_manager,
                                 ProfileOAuth2TokenService* token_service)
    : signin_manager_(signin_manager), token_service_(token_service) {
  registry_.AddInterface<mojom::IdentityManager>(
      base::Bind(&IdentityService::Create, base::Unretained(this)));
}

IdentityService::~IdentityService() {}

void IdentityService::OnStart() {}

void IdentityService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(source_info, interface_name,
                          std::move(interface_pipe));
}

void IdentityService::Create(const service_manager::BindSourceInfo& source_info,
                             mojom::IdentityManagerRequest request) {
  IdentityManager::Create(std::move(request), signin_manager_, token_service_);
}

}  // namespace identity
