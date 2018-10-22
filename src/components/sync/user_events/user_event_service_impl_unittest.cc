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
#include "components/sync/driver/fake_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
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

std::unique_ptr<UserEventSpecifics> AsConsent(
    std::unique_ptr<UserEventSpecifics> specifics) {
  specifics->mutable_user_consent()->set_account_id("account_id");
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

// TODO(vitaliii): Merge this into FakeSyncService and use it instead.
class TestSyncService : public FakeSyncService {
 public:
  TestSyncService(bool is_engine_initialized,
                  bool is_using_secondary_passphrase,
                  ModelTypeSet preferred_data_types)
      : is_engine_initialized_(is_engine_initialized),
        is_using_secondary_passphrase_(is_using_secondary_passphrase),
        preferred_data_types_(preferred_data_types) {}

  TransportState GetTransportState() const override {
    return is_engine_initialized_ ? TransportState::ACTIVE
                                  : TransportState::INITIALIZING;
  }

  bool IsUsingSecondaryPassphrase() const override {
    return is_using_secondary_passphrase_;
  }

  ModelTypeSet GetPreferredDataTypes() const override {
    return preferred_data_types_;
  }

 private:
  bool is_engine_initialized_;
  bool is_using_secondary_passphrase_;
  ModelTypeSet preferred_data_types_;
};

class TestGlobalIdMapper : public GlobalIdMapper {
  void AddGlobalIdChangeObserver(GlobalIdChange callback) override {}
  int64_t GetLatestGlobalId(int64_t global_id) override { return global_id; }
};

class UserEventServiceImplTest : public testing::Test {
 protected:
  UserEventServiceImplTest()
      : field_trial_list_(nullptr),
        sync_service_(true, false, {HISTORY_DELETE_DIRECTIVES}) {
    ON_CALL(mock_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
  }

  std::unique_ptr<UserEventSyncBridge> MakeBridge() {
    return std::make_unique<UserEventSyncBridge>(
        ModelTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        mock_processor_.CreateForwardingProcessor(), &mapper_);
  }

  void SetIsSeparateConsentTypeEnabledFeature(bool new_value) {
    feature_list_.InitWithFeatureState(switches::kSyncUserConsentSeparateType,
                                       new_value);
  }

  TestSyncService* sync_service() { return &sync_service_; }
  MockModelTypeChangeProcessor* mock_processor() { return &mock_processor_; }

 private:
  base::MessageLoop message_loop_;
  base::FieldTrialList field_trial_list_;
  TestSyncService sync_service_;
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
  SetIsSeparateConsentTypeEnabledFeature(false);

  UserEventServiceImpl service(sync_service(), MakeBridge());
  EXPECT_CALL(*mock_processor(), Put(_, _, _));
  service.RecordUserEvent(AsTest(Event()));
}

TEST_F(UserEventServiceImplTest, ShouldRecordNoHistory) {
  SetIsSeparateConsentTypeEnabledFeature(false);

  TestSyncService no_history_sync_service(true, false, ModelTypeSet());
  UserEventServiceImpl service(&no_history_sync_service, MakeBridge());

  // Only record events without navigation ids when history sync is off.
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  service.RecordUserEvent(WithNav(AsTest(Event())));
  EXPECT_CALL(*mock_processor(), Put(_, _, _));
  service.RecordUserEvent(AsTest(Event()));
}

TEST_F(UserEventServiceImplTest, ShouldRecordUserConsentNoHistory) {
  SetIsSeparateConsentTypeEnabledFeature(false);

  TestSyncService no_history_sync_service(true, false, ModelTypeSet());
  UserEventServiceImpl service(&no_history_sync_service, MakeBridge());

  // UserConsent recording doesn't need history sync to be enabled.
  EXPECT_CALL(*mock_processor(), Put(_, _, _));
  service.RecordUserEvent(AsConsent(Event()));
}

TEST_F(UserEventServiceImplTest, ShouldRecordPassphrase) {
  SetIsSeparateConsentTypeEnabledFeature(false);

  TestSyncService passphrase_sync_service(true, true,
                                          {HISTORY_DELETE_DIRECTIVES});
  UserEventServiceImpl service(&passphrase_sync_service, MakeBridge());

  // Only record events without navigation ids when a passphrase is used.
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  service.RecordUserEvent(WithNav(AsTest(Event())));

  EXPECT_CALL(*mock_processor(), Put(_, _, _));
  service.RecordUserEvent(AsTest(Event()));
}

TEST_F(UserEventServiceImplTest, ShouldRecordEngineOff) {
  SetIsSeparateConsentTypeEnabledFeature(false);

  TestSyncService engine_not_initialized_sync_service(
      false, false, {HISTORY_DELETE_DIRECTIVES});
  UserEventServiceImpl service(&engine_not_initialized_sync_service,
                               MakeBridge());

  // Only record events without navigation ids when the engine is off.
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  service.RecordUserEvent(WithNav(AsTest(Event())));

  EXPECT_CALL(*mock_processor(), Put(_, _, _));
  service.RecordUserEvent(AsTest(Event()));
}

TEST_F(UserEventServiceImplTest, ShouldRecordEmpty) {
  UserEventServiceImpl service(sync_service(), MakeBridge());

  // All untyped events should always be ignored.
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  service.RecordUserEvent(Event());
  service.RecordUserEvent(WithNav(Event()));
}

TEST_F(UserEventServiceImplTest, ShouldRecordHasNavigationId) {
  SetIsSeparateConsentTypeEnabledFeature(false);

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
  SetIsSeparateConsentTypeEnabledFeature(false);

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
  SetIsSeparateConsentTypeEnabledFeature(false);

  variations::AssociateGoogleVariationID(variations::CHROME_SYNC_EVENT_LOGGER,
                                         "trial", "group", 123);
  base::FieldTrialList::CreateFieldTrial("trial", "group");
  base::FieldTrialList::FindFullName("trial");

  EXPECT_CALL(*mock_processor(), Put(_, HasFieldTrialVariationIds(123u), _));
  UserEventServiceImpl service(sync_service(), MakeBridge());
}

TEST_F(
    UserEventServiceImplTest,
    WithConsentsTypeShouldRecordWhenBothHistoryAndEventsDatatypesAreEnabled) {
  SetIsSeparateConsentTypeEnabledFeature(true);

  TestSyncService sync_service(
      /*is_engine_initialized=*/true,
      /*is_using_secondary_passphrase=*/false,
      /*preferred_data_types=*/{HISTORY_DELETE_DIRECTIVES, USER_EVENTS});

  UserEventServiceImpl service(&sync_service, MakeBridge());
  EXPECT_CALL(*mock_processor(), Put(_, _, _));
  service.RecordUserEvent(AsTest(Event()));
}

TEST_F(UserEventServiceImplTest,
       WithConsentsTypeShouldNotRecordWhenEventsDatatypeIsDisabled) {
  SetIsSeparateConsentTypeEnabledFeature(true);

  TestSyncService sync_service(
      /*is_engine_initialized=*/true,
      /*is_using_secondary_passphrase=*/false,
      /*preferred_data_types=*/{HISTORY_DELETE_DIRECTIVES});

  UserEventServiceImpl service(&sync_service, MakeBridge());
  // USER_EVENTS type is disabled, thus, they should not be recorded.
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  service.RecordUserEvent(AsTest(Event()));
}

TEST_F(UserEventServiceImplTest,
       WithConsentsTypeShouldNotRecordWhenHistoryDatatypeIsDisabled) {
  SetIsSeparateConsentTypeEnabledFeature(true);

  TestSyncService sync_service(
      /*is_engine_initialized=*/true,
      /*is_using_secondary_passphrase=*/false,
      /*preferred_data_types=*/{USER_EVENTS});

  UserEventServiceImpl service(&sync_service, MakeBridge());
  // Even though USER_EVENTS type is enabled, events cannot be recorded when
  // history sync is disabled.
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  // Use |WithNav| because only events with navigation id depend on history.
  service.RecordUserEvent(WithNav(Event()));
}

TEST_F(UserEventServiceImplTest,
       WithConsentsTypeShouldNotRecordWhenEngineIsNotInitialized) {
  SetIsSeparateConsentTypeEnabledFeature(true);

  TestSyncService sync_service(
      /*is_engine_initialized=*/false,
      /*is_using_secondary_passphrase=*/false,
      /*preferred_data_types=*/{HISTORY_DELETE_DIRECTIVES, USER_EVENTS});

  UserEventServiceImpl service(&sync_service, MakeBridge());
  // Even though USER_EVENTS type is enabled, events cannot be recorded because
  // we can't trust uninitialized engine.
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  service.RecordUserEvent(AsTest(Event()));
}

TEST_F(UserEventServiceImplTest,
       WithConsentsTypeShouldNotRecordWhenPassphraseIsUsed) {
  SetIsSeparateConsentTypeEnabledFeature(true);

  TestSyncService sync_service(
      /*is_engine_initialized=*/true,
      /*is_using_secondary_passphrase=*/true,
      /*preferred_data_types=*/{HISTORY_DELETE_DIRECTIVES, USER_EVENTS});

  UserEventServiceImpl service(&sync_service, MakeBridge());
  // Even though USER_EVENTS type is enabled, events cannot be recorded
  // because custom passphrase is used.
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  service.RecordUserEvent(AsTest(Event()));
}

}  // namespace

}  // namespace syncer
