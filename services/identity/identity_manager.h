// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_IDENTITY_MANAGER_H_
#define SERVICES_IDENTITY_IDENTITY_MANAGER_H_

#include "services/identity/public/interfaces/identity_manager.mojom.h"

class SigninManagerBase;

namespace identity {

class IdentityManager : public mojom::IdentityManager {
 public:
  static void Create(mojom::IdentityManagerRequest request,
                     SigninManagerBase* signin_manager);

  IdentityManager(SigninManagerBase* signin_manager);
  ~IdentityManager() override;

 private:
  // mojom::IdentityManager:
  void GetPrimaryAccountId(
      const GetPrimaryAccountIdCallback& callback) override;

  SigninManagerBase* signin_manager_;
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_IDENTITY_MANAGER_H_
