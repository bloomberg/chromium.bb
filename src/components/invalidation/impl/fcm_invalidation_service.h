// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_SERVICE_H_
#define COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_SERVICE_H_

#include "base/macros.h"
#include "components/invalidation/impl/fcm_invalidation_service_base.h"
#include "components/invalidation/public/identity_provider.h"

namespace invalidation {

// This concrete implementation of FCMInvalidationServiceBase starts the
// invalidation service machinery once an account is signed in and conversely
// stops it when the account is signed out.
class FCMInvalidationService : public FCMInvalidationServiceBase,
                               public IdentityProvider::Observer {
 public:
  FCMInvalidationService(IdentityProvider* identity_provider,
                         FCMNetworkHandlerCallback fcm_network_handler_callback,
                         PerUserTopicSubscriptionManagerCallback
                             per_user_topic_subscription_manager_callback,
                         instance_id::InstanceIDDriver* instance_id_driver,
                         PrefService* pref_service,
                         const std::string& sender_id = {});
  ~FCMInvalidationService() override;

  void Init() override;

  void RequestDetailedStatus(
      base::RepeatingCallback<void(const base::DictionaryValue&)> caller)
      const override;

  // IdentityProvider::Observer implementation.
  void OnActiveAccountRefreshTokenUpdated() override;
  void OnActiveAccountLogin() override;
  void OnActiveAccountLogout() override;

 protected:
  friend class FCMInvalidationServiceTestDelegate;

  base::DictionaryValue CollectDebugData() const override;

 private:
  struct Diagnostics {
    base::Time active_account_login;
    base::Time active_account_token_updated;
    base::Time active_account_logged_out;
    bool was_already_started_on_login = false;
    bool was_ready_to_start_on_login = false;
    CoreAccountId active_account_id;
  };

  bool IsReadyToStart();

  IdentityProvider* const identity_provider_;
  Diagnostics diagnostic_info_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(FCMInvalidationService);
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_SERVICE_H_
