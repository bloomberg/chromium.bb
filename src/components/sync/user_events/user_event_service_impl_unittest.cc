// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/user_events/user_event_service_impl.h"

#include <utility>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model/model_type_store_test_util.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/user_events/user_event_sync_bridge.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::ScopedFeatureList;
using sync_pb::UserEventSpecifics;
using testing::_;

namespace syncer {

namespace {

std::unique_ptr<UserEventSpecifics> Event() {
  return std::make_unique<UserEventSpecifics>();
}

std::unique_ptr<UserEventSpecifics> AsTest(
    std::unique_ptr<UserEventSpecifics> specifics) {
  specifics->mutable_test_event();
  return specifics;
}

std::unique_ptr<UserEventSpecifics> AsDetection(
    std::unique_ptr<UserEventSpecifics> specifics) {
  specifics->mutable_language_detection_event();
  return specifics;
}

std::unique_ptr<UserEventSpecifics> AsTrial(
    std::unique_ptr<UserEventSpecifics> specifics) {
  specifics->mutable_field_trial_event();
  return specifics;
}

std::unique_ptr<UserEventSpecifics> WithNav(
    std::unique_ptr<UserEventSpecifics> specifics,
    int64_t navigation_id = 1) {
  specifics->set_navigation_id(navigation_id);
  return specifics;
}

MATCHER_P(HasFieldTrialVariationIds, expected_variation_id, "") {
  const UserEventSpecifics& specifics = arg->specifics.user_event();
  return specifics.field_trial_event().variation_ids_size() == 1 &&
         specifics.field_trial_event().variation_ids(0) ==
             expected_variation_id;
}

class TestGlobalIdMapper : public GlobalIdMapper {
  void AddGlobalIdChangeObserver(GlobalIdChange callback) override {}
  int64_t GetLatestGlobalId(int64_t global_id) override { return global_id; }
};

class UserEventServiceImplTest : public testing::Test {
 protected:
  UserEventServiceImplTest() : field_trial_list_(nullptr) {
    sync_service_.SetPreferredDataTypes(
        {HISTORY_DELETE_DIRECTIVES, USER_EVENTS});
    ON_CALL(mock_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    ON_CALL(mock_processor_, TrackedAccountId())
        .WillByDefault(testing::Return("account_id"));
  }

  std::unique_ptr<UserEventSyncBridge> MakeBridge() {
    return std::make_unique<UserEventSyncBridge>(
        ModelTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        mock_processor_.CreateForwardingProcessor(), &mapper_);
  }

  syncer::TestSyncService* sync_service() { return &sync_service_; }
  MockModelTypeChangeProcessor* mock_processor() { return &mock_processor_; }

 private:
  base::MessageLoop message_loop_;
  base::FieldTrialList field_trial_list_;
  syncer::TestSyncService sync_service_;
  testing::NiceMock<MockModelTypeChangeProcessor> mock_processor_;
  TestGlobalIdMapper mapper_;

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(UserEventServiceImplTest, MightRecordEventsFeatureEnabled) {
  // All conditions are met, might record.
  EXPECT_TRUE(UserEventServiceImpl::MightRecordEvents(false, sync_service()));
  // No sync service, will not record.
  EXPECT_FALSE(UserEventServiceImpl::MightRecordEvents(false, nullptr));
  // Off the record, will not record.
  EXPECT_FALSE(UserEventServiceImpl::MightRecordEvents(true, sync_service()));
}

TEST_F(UserEventServiceImplTest, MightRecordEventsFeatureDisabled) {
  // Will not record because the default on feature is overridden.
  ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(switches::kSyncUserEvents);
  EXPECT_FALSE(UserEventServiceImpl::MightRecordEvents(false, sync_service()));
}

TEST_F(UserEventServiceImplTest, ShouldRecord) {
  UserEventServiceImpl service(sync_service(), MakeBridge());
  EXPECT_CALL(*mock_processor(), Put(_, _, _));
  service.RecordUserEvent(AsTest(Event()));
}

TEST_F(UserEventServiceImplTest,
       ShouldOnlyRecordEventsWithoutNavIdWhenHistorySyncIsDisabled) {
  sync_service()->SetPreferredDataTypes({USER_EVENTS});
  UserEventServiceImpl service(sync_service(), MakeBridge());

  // Only record events without navigation ids when history sync is off.
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  service.RecordUserEvent(WithNav(AsTest(Event())));
  EXPECT_CALL(*mock_processor(), Put(_, _, _));
  service.RecordUserEvent(AsTest(Event()));
}

TEST_F(UserEventServiceImplTest, ShouldNotRecordWhenPassphraseIsUsed) {
  sync_service()->SetIsUsingSecondaryPassphrase(true);
  UserEventServiceImpl service(sync_service(), MakeBridge());

  // Do not record events when a passphrase is used.
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  service.RecordUserEvent(WithNav(AsTest(Event())));
  service.RecordUserEvent(AsTest(Event()));
}

TEST_F(UserEventServiceImplTest, ShouldNotRecordWhenEngineIsNotInitialized) {
  sync_service()->SetTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  UserEventServiceImpl service(sync_service(), MakeBridge());

  // Do not record events when the engine is off.
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  service.RecordUserEvent(WithNav(AsTest(Event())));
  service.RecordUserEvent(AsTest(Event()));
}

TEST_F(UserEventServiceImplTest, ShouldNotRecordEmptyEvents) {
  UserEventServiceImpl service(sync_service(), MakeBridge());

  // All untyped events should always be ignored.
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  service.RecordUserEvent(Event());
  service.RecordUserEvent(WithNav(Event()));
}

TEST_F(UserEventServiceImplTest, ShouldRecordHasNavigationId) {
  UserEventServiceImpl service(sync_service(), MakeBridge());

  // Verify logic for types that might or might not have a navigation id.
  EXPECT_CALL(*mock_processor(), Put(_, _, _));
  service.RecordUserEvent(AsTest(Event()));
  EXPECT_CALL(*mock_processor(), Put(_, _, _));
  service.RecordUserEvent(WithNav(AsTest(Event())));

  // Verify logic for types that must have a navigation id.
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  service.RecordUserEvent(AsDetection(Event()));
  EXPECT_CALL(*mock_processor(), Put(_, _, _));
  service.RecordUserEvent(WithNav(AsDetection(Event())));

  // Verify logic for types that cannot have a navigation id.
  EXPECT_CALL(*mock_processor(), Put(_, _, _));
  service.RecordUserEvent(AsTrial(Event()));
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  service.RecordUserEvent(WithNav(AsTrial(Event())));
}

TEST_F(UserEventServiceImplTest, SessionIdIsDifferent) {
  std::vector<int64_t> put_session_ids;
  ON_CALL(*mock_processor(), Put(_, _, _))
      .WillByDefault([&](const std::string& storage_key,
                         const std::unique_ptr<EntityData> entity_data,
                         MetadataChangeList* metadata_change_list) {
        put_session_ids.push_back(
            entity_data->specifics.user_event().session_id());
      });

  UserEventServiceImpl service1(sync_service(), MakeBridge());
  service1.RecordUserEvent(AsTest(Event()));

  UserEventServiceImpl service2(sync_service(), MakeBridge());
  service2.RecordUserEvent(AsTest(Event()));

  ASSERT_EQ(2U, put_session_ids.size());
  EXPECT_NE(put_session_ids[0], put_session_ids[1]);
}

TEST_F(UserEventServiceImplTest, FieldTrial) {
  variations::AssociateGoogleVariationID(variations::CHROME_SYNC_EVENT_LOGGER,
                                         "trial", "group", 123);
  base::FieldTrialList::CreateFieldTrial("trial", "group");
  base::FieldTrialList::FindFullName("trial");

  EXPECT_CALL(*mock_processor(), Put(_, HasFieldTrialVariationIds(123u), _));
  UserEventServiceImpl service(sync_service(), MakeBridge());
}

TEST_F(UserEventServiceImplTest, ShouldNotRecordWhenEventsDatatypeIsDisabled) {
  sync_service()->SetPreferredDataTypes({HISTORY_DELETE_DIRECTIVES});
  UserEventServiceImpl service(sync_service(), MakeBridge());
  // USER_EVENTS type is disabled, thus, they should not be recorded.
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  service.RecordUserEvent(AsTest(Event()));
}

}  // namespace

}  // namespace syncer
