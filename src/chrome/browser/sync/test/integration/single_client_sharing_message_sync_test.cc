// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/test/mock_callback.h"
#include "chrome/browser/sharing/sharing_message_bridge.h"
#include "chrome/browser/sharing/sharing_message_bridge_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/test/fake_server/fake_server_http_post_provider.h"
#include "content/public/test/browser_test.h"

namespace {

using sync_pb::SharingMessageSpecifics;

MATCHER_P(HasErrorCode, expected_error_code, "") {
  return arg.error_code() == expected_error_code;
}

class NextCycleIterationChecker : public SingleClientStatusChangeChecker {
 public:
  explicit NextCycleIterationChecker(syncer::ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {
    last_synced_time_ = SingleClientStatusChangeChecker::service()
                            ->GetLastSyncedTimeForDebugging();
  }

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting server side for next sync cycle.";

    if (last_synced_time_ == service()->GetLastSyncedTimeForDebugging()) {
      return false;
    }

    return true;
  }

 private:
  base::Time last_synced_time_;
};

class BackedOffSharingMessageChecker : public SingleClientStatusChangeChecker {
 public:
  explicit BackedOffSharingMessageChecker(syncer::ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    return service()->GetBackedOffDataTypes().Has(syncer::SHARING_MESSAGE);
  }
};

class SharingMessageEqualityChecker : public SingleClientStatusChangeChecker {
 public:
  SharingMessageEqualityChecker(
      syncer::ProfileSyncService* service,
      fake_server::FakeServer* fake_server,
      std::vector<SharingMessageSpecifics> expected_specifics)
      : SingleClientStatusChangeChecker(service),
        fake_server_(fake_server),
        expected_specifics_(std::move(expected_specifics)) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting server side SHARING_MESSAGE to match expected.";
    std::vector<sync_pb::SyncEntity> entities =
        fake_server_->GetSyncEntitiesByModelType(syncer::SHARING_MESSAGE);

    // |entities.size()| is only going to grow, if |entities.size()| ever
    // becomes bigger then all hope is lost of passing, stop now.
    EXPECT_GE(expected_specifics_.size(), entities.size());

    if (expected_specifics_.size() > entities.size()) {
      return false;
    }

    // Number of events on server matches expected, exit condition can be
    // satisfied. Let's verify that content matches as well. It is safe to
    // modify |expected_specifics_|.
    for (const sync_pb::SyncEntity& entity : entities) {
      const SharingMessageSpecifics& server_specifics =
          entity.specifics().sharing_message();
      // It is likely to have the same order, it is ok to use find here.
      auto iter = std::find_if(
          expected_specifics_.begin(), expected_specifics_.end(),
          [&server_specifics](const SharingMessageSpecifics& specifics) {
            return specifics.payload() == server_specifics.payload();
          });
      EXPECT_TRUE(iter != expected_specifics_.end());
      if (iter == expected_specifics_.end())
        return false;
      expected_specifics_.erase(iter);
    }

    return true;
  }

 private:
  fake_server::FakeServer* fake_server_;
  std::vector<SharingMessageSpecifics> expected_specifics_;
};

class SingleClientSharingMessageSyncTest : public SyncTest {
 public:
  SingleClientSharingMessageSyncTest() : SyncTest(SINGLE_CLIENT) {
    DisableVerifier();
  }

  bool WaitForSharingMessage(
      std::vector<SharingMessageSpecifics> expected_specifics) {
    return SharingMessageEqualityChecker(GetSyncService(0), GetFakeServer(),
                                         std::move(expected_specifics))
        .Wait();
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientSharingMessageSyncTest, ShouldSubmit) {
  base::MockOnceCallback<void(const sync_pb::SharingMessageCommitError&)>
      callback;
  EXPECT_CALL(callback,
              Run(HasErrorCode(sync_pb::SharingMessageCommitError::NONE)));

  ASSERT_TRUE(SetupSync());
  ASSERT_EQ(0u, GetFakeServer()
                    ->GetSyncEntitiesByModelType(syncer::SHARING_MESSAGE)
                    .size());

  SharingMessageBridge* sharing_message_bridge =
      SharingMessageBridgeFactory::GetForBrowserContext(GetProfile(0));
  SharingMessageSpecifics specifics;
  specifics.set_payload("payload");
  sharing_message_bridge->SendSharingMessage(
      std::make_unique<SharingMessageSpecifics>(specifics), callback.Get());

  EXPECT_TRUE(WaitForSharingMessage({specifics}));
}

// ChromeOS does not support late signin after profile creation, so the test
// below does not apply, at least in the current form.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SingleClientSharingMessageSyncTest,
                       ShouldSubmitInTransportMode) {
  // We avoid calling SetupSync(), because we don't want to turn on full sync,
  // only sign in such that the standalone transport starts.
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive())
      << "Full sync should be disabled";
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::SHARING_MESSAGE));

  base::MockOnceCallback<void(const sync_pb::SharingMessageCommitError&)>
      callback;
  EXPECT_CALL(callback,
              Run(HasErrorCode(sync_pb::SharingMessageCommitError::NONE)));

  SharingMessageBridge* sharing_message_bridge =
      SharingMessageBridgeFactory::GetForBrowserContext(GetProfile(0));
  SharingMessageSpecifics specifics;
  specifics.set_payload("payload");
  sharing_message_bridge->SendSharingMessage(
      std::make_unique<SharingMessageSpecifics>(specifics), callback.Get());

  EXPECT_TRUE(WaitForSharingMessage({specifics}));
}
#endif  // !defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(SingleClientSharingMessageSyncTest,
                       ShouldPropagateCommitFailure) {
  base::MockOnceCallback<void(const sync_pb::SharingMessageCommitError&)>
      callback;
  EXPECT_CALL(
      callback,
      Run(HasErrorCode(sync_pb::SharingMessageCommitError::SYNC_SERVER_ERROR)));

  ASSERT_TRUE(SetupSync());
  GetFakeServer()->TriggerCommitError(sync_pb::SyncEnums::TRANSIENT_ERROR);

  SharingMessageBridge* sharing_message_bridge =
      SharingMessageBridgeFactory::GetForBrowserContext(GetProfile(0));
  SharingMessageSpecifics specifics;
  specifics.set_payload("payload");
  sharing_message_bridge->SendSharingMessage(
      std::make_unique<SharingMessageSpecifics>(specifics), callback.Get());

  ASSERT_TRUE(NextCycleIterationChecker(GetSyncService(0)).Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientSharingMessageSyncTest,
                       ShouldStopDataTypeWhenBackedOff) {
  base::MockRepeatingCallback<void(const sync_pb::SharingMessageCommitError&)>
      callback;

  ASSERT_TRUE(SetupSync());
  SharingMessageBridge* sharing_message_bridge =
      SharingMessageBridgeFactory::GetForBrowserContext(GetProfile(0));
  SharingMessageSpecifics specifics;
  specifics.set_payload("payload");
  sharing_message_bridge->SendSharingMessage(
      std::make_unique<SharingMessageSpecifics>(specifics), callback.Get());

  EXPECT_CALL(
      callback,
      Run(HasErrorCode(sync_pb::SharingMessageCommitError::SYNC_TURNED_OFF)));
  ASSERT_FALSE(
      GetSyncService(0)->GetBackedOffDataTypes().Has(syncer::SHARING_MESSAGE));

  // Run the data type into backed off state before the message gets sent.
  fake_server::FakeServerHttpPostProvider::DisableNetwork();

  ASSERT_TRUE(BackedOffSharingMessageChecker(GetSyncService(0)).Wait());

  EXPECT_TRUE(
      GetSyncService(0)->GetBackedOffDataTypes().Has(syncer::SHARING_MESSAGE));
  EXPECT_CALL(
      callback,
      Run(HasErrorCode(sync_pb::SharingMessageCommitError::SYNC_TURNED_OFF)));
  sharing_message_bridge->SendSharingMessage(
      std::make_unique<SharingMessageSpecifics>(specifics), callback.Get());
}

IN_PROC_BROWSER_TEST_F(SingleClientSharingMessageSyncTest,
                       ShouldCleanPendingMessagesAfterSyncPaused) {
  base::MockOnceCallback<void(const sync_pb::SharingMessageCommitError&)>
      callback;
  EXPECT_CALL(
      callback,
      Run(HasErrorCode(sync_pb::SharingMessageCommitError::SYNC_TURNED_OFF)));

  ASSERT_TRUE(SetupSync());

  SharingMessageBridge* sharing_message_bridge =
      SharingMessageBridgeFactory::GetForBrowserContext(GetProfile(0));
  SharingMessageSpecifics specifics;
  specifics.set_payload("payload");
  sharing_message_bridge->SendSharingMessage(
      std::make_unique<SharingMessageSpecifics>(specifics), callback.Get());

  GetClient(0)->StopSyncServiceWithoutClearingData();
  GetClient(0)->StartSyncService();
  ASSERT_TRUE(NextCycleIterationChecker(GetSyncService(0)).Wait());

  EXPECT_TRUE(GetFakeServer()
                  ->GetSyncEntitiesByModelType(syncer::SHARING_MESSAGE)
                  .empty());
}

IN_PROC_BROWSER_TEST_F(SingleClientSharingMessageSyncTest,
                       ShouldCleanPendingMessagesAfterSyncTurnedOff) {
  base::MockOnceCallback<void(const sync_pb::SharingMessageCommitError&)>
      callback;
  EXPECT_CALL(
      callback,
      Run(HasErrorCode(sync_pb::SharingMessageCommitError::SYNC_TURNED_OFF)));

  ASSERT_TRUE(SetupSync());

  SharingMessageBridge* sharing_message_bridge =
      SharingMessageBridgeFactory::GetForBrowserContext(GetProfile(0));
  SharingMessageSpecifics specifics;
  specifics.set_payload("payload");
  sharing_message_bridge->SendSharingMessage(
      std::make_unique<SharingMessageSpecifics>(specifics), callback.Get());

  GetClient(0)->StopSyncServiceAndClearData();
  GetClient(0)->StartSyncService();
  ASSERT_TRUE(NextCycleIterationChecker(GetSyncService(0)).Wait());

  EXPECT_TRUE(GetFakeServer()
                  ->GetSyncEntitiesByModelType(syncer::SHARING_MESSAGE)
                  .empty());
}

}  // namespace
