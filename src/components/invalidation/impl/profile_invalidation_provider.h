// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_PROFILE_INVALIDATION_PROVIDER_H_
#define COMPONENTS_INVALIDATION_IMPL_PROFILE_INVALIDATION_PROVIDER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/keyed_service/core/keyed_service.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace invalidation {

class InvalidationService;

// A KeyedService that owns an InvalidationService.
class ProfileInvalidationProvider : public KeyedService {
 public:
  explicit ProfileInvalidationProvider(
      std::unique_ptr<InvalidationService> invalidation_service,
      std::unique_ptr<IdentityProvider> identity_provider);
  ~ProfileInvalidationProvider() override;

  InvalidationService* GetInvalidationService();

  IdentityProvider* GetIdentityProvider();

  // KeyedService:
  void Shutdown() override;

  // Register prefs to be used by per-Profile instances of this class which
  // store invalidation state in Profile prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  // The order of members |identity_provider_| and |invalidation_service_|
  // shouldn't change. The reason is  that service  has a pointer to the
  // provider. So this particular order in the declarations ensures order
  // in destruction.
  std::unique_ptr<IdentityProvider> identity_provider_;
  std::unique_ptr<InvalidationService> invalidation_service_;

  DISALLOW_COPY_AND_ASSIGN(ProfileInvalidationProvider);
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_PROFILE_INVALIDATION_PROVIDER_H_
