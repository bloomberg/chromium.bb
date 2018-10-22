// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_invalidator.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/invalidation/impl/fake_invalidation_handler.h"
#include "components/invalidation/impl/invalidator_test_template.h"
#include "components/invalidation/impl/json_unsafe_parser.h"
#include "components/invalidation/impl/per_user_topic_registration_manager.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/invalidation/impl/push_client_channel.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "components/prefs/testing_pref_service.h"
#include "net/url_request/url_request_test_util.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class FCMInvalidatorTestDelegate {
 public:
  FCMInvalidatorTestDelegate() {
    PerUserTopicRegistrationManager::RegisterProfilePrefs(
        pref_service_.registry());
  }

  ~FCMInvalidatorTestDelegate() { DestroyInvalidator(); }

  void CreateInvalidator(const std::string&,
                         const std::string&,
                         const base::WeakPtr<InvalidationStateTracker>&) {
    DCHECK(!invalidator_);
    auto network_channel = std::make_unique<FCMSyncNetworkChannel>();
    identity_provider_ =
        std::make_unique<invalidation::ProfileIdentityProvider>(
            identity_test_env_.identity_manager());
    invalidator_.reset(new FCMInvalidator(
        std::move(network_channel), identity_provider_.get(), &pref_service_,
        &url_loader_factory_,
        base::BindRepeating(&syncer::JsonUnsafeParser::Parse)));
  }

  Invalidator* GetInvalidator() { return invalidator_.get(); }

  void DestroyInvalidator() {
    base::RunLoop().RunUntilIdle();
    invalidator_.reset();
  }

  void WaitForInvalidator() { base::RunLoop().RunUntilIdle(); }

  void TriggerOnInvalidatorStateChange(InvalidatorState state) {
    invalidator_->OnInvalidatorStateChange(state);
  }

  void TriggerOnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map) {
    invalidator_->OnInvalidate(
        ConvertObjectIdInvalidationMapToTopicInvalidationMap(invalidation_map));
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<FCMInvalidator> invalidator_;
  identity::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<invalidation::IdentityProvider> identity_provider_;
  network::TestURLLoaderFactory url_loader_factory_;
  TestingPrefServiceSimple pref_service_;
};

INSTANTIATE_TYPED_TEST_CASE_P(FCMInvalidatorTest,
                              InvalidatorTest,
                              FCMInvalidatorTestDelegate);

}  // namespace

}  // namespace syncer
