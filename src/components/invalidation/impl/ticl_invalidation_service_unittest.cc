// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/ticl_invalidation_service.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/invalidation/impl/fake_invalidation_state_tracker.h"
#include "components/invalidation/impl/fake_invalidator.h"
#include "components/invalidation/impl/gcm_invalidation_bridge.h"
#include "components/invalidation/impl/invalidation_service_test_template.h"
#include "components/invalidation/impl/invalidation_state_tracker.h"
#include "components/invalidation/impl/invalidator.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {

namespace {

class FakeTiclSettingsProvider : public TiclSettingsProvider {
 public:
  FakeTiclSettingsProvider();
  ~FakeTiclSettingsProvider() override;

  // TiclSettingsProvider:
  bool UseGCMChannel() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeTiclSettingsProvider);
};

FakeTiclSettingsProvider::FakeTiclSettingsProvider() {
}

FakeTiclSettingsProvider::~FakeTiclSettingsProvider() {
}

bool FakeTiclSettingsProvider::UseGCMChannel() const {
  return false;
}

}  // namespace

class TiclInvalidationServiceTestDelegate {
 public:
  TiclInvalidationServiceTestDelegate() {}

  ~TiclInvalidationServiceTestDelegate() {
  }

  void CreateInvalidationService() {
    CreateUninitializedInvalidationService();
    InitializeInvalidationService();
  }

  void CreateUninitializedInvalidationService() {
    gcm_driver_ = std::make_unique<gcm::FakeGCMDriver>();
    identity_provider_ = std::make_unique<ProfileIdentityProvider>(
        identity_test_env_.identity_manager());
    DCHECK(identity_provider_);
    invalidation_service_ = std::make_unique<TiclInvalidationService>(
        "TestUserAgent", identity_provider_.get(),
        std::unique_ptr<TiclSettingsProvider>(new FakeTiclSettingsProvider),
        gcm_driver_.get(), nullptr, nullptr,
        network::TestNetworkConnectionTracker::GetInstance());
  }

  void InitializeInvalidationService() {
    fake_invalidator_ = new syncer::FakeInvalidator();
    invalidation_service_->InitForTest(
        base::WrapUnique(new syncer::FakeInvalidationStateTracker),
        fake_invalidator_);
  }

  InvalidationService* GetInvalidationService() {
    return invalidation_service_.get();
  }

  void DestroyInvalidationService() {
    invalidation_service_.reset();
  }

  void TriggerOnInvalidatorStateChange(syncer::InvalidatorState state) {
    fake_invalidator_->EmitOnInvalidatorStateChange(state);
  }

  void TriggerOnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) {
    fake_invalidator_->EmitOnIncomingInvalidation(invalidation_map);
  }

  identity::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<gcm::GCMDriver> gcm_driver_;
  std::unique_ptr<invalidation::IdentityProvider> identity_provider_;
  syncer::FakeInvalidator* fake_invalidator_;  // Owned by the service.

  // The service has to be below the provider since the service keeps
  // a non-owned pointer to the provider.
  std::unique_ptr<TiclInvalidationService> invalidation_service_;
};

INSTANTIATE_TYPED_TEST_CASE_P(
    TiclInvalidationServiceTest, InvalidationServiceTest,
    TiclInvalidationServiceTestDelegate);

namespace internal {

class FakeCallbackContainer {
  public:
    FakeCallbackContainer() : called_(false),
                              weak_ptr_factory_(this) {}

    void FakeCallback(const base::DictionaryValue& value) {
      called_ = true;
    }

    bool called_;
    base::WeakPtrFactory<FakeCallbackContainer> weak_ptr_factory_;
};

}  // namespace internal

// Test that requesting for detailed status doesn't crash even if the
// underlying invalidator is not initialized.
TEST(TiclInvalidationServiceLoggingTest, DetailedStatusCallbacksWork) {
  std::unique_ptr<TiclInvalidationServiceTestDelegate> delegate(
      new TiclInvalidationServiceTestDelegate());

  delegate->CreateUninitializedInvalidationService();
  invalidation::InvalidationService* const invalidator =
      delegate->GetInvalidationService();

  internal::FakeCallbackContainer fake_container;
  invalidator->RequestDetailedStatus(
      base::Bind(&internal::FakeCallbackContainer::FakeCallback,
                 fake_container.weak_ptr_factory_.GetWeakPtr()));
  EXPECT_FALSE(fake_container.called_);

  delegate->InitializeInvalidationService();

  invalidator->RequestDetailedStatus(
      base::Bind(&internal::FakeCallbackContainer::FakeCallback,
                 fake_container.weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(fake_container.called_);
}

}  // namespace invalidation
