// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/sync_invalidations_service_factory.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/device_info_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/driver/glue/sync_transport_data_prefs.h"
#include "components/sync/invalidations/switches.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/test/fake_server/bookmark_entity_builder.h"
#include "components/sync/test/fake_server/entity_builder_factory.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/device_info_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using bookmarks::BookmarkNode;
using bookmarks_helper::AddFolder;
using bookmarks_helper::GetBookmarkBarNode;
using bookmarks_helper::ServerBookmarksEqualityChecker;
using testing::AllOf;
using testing::ElementsAre;
using testing::Not;
using testing::NotNull;
using testing::SizeIs;
using testing::UnorderedElementsAre;

const char kSyncedBookmarkURL[] = "http://www.mybookmark.com";

MATCHER_P(HasBeenUpdatedAfter, last_updated_timestamp, "") {
  return arg.specifics().device_info().last_updated_timestamp() >
         last_updated_timestamp;
}

MATCHER_P(HasCacheGuid, expected_cache_guid, "") {
  return arg.specifics().device_info().cache_guid() == expected_cache_guid;
}

MATCHER_P(InterestedDataTypesAre, expected_data_types, "") {
  syncer::ModelTypeSet data_types;
  for (const int field_number : arg.specifics()
                                    .device_info()
                                    .invalidation_fields()
                                    .interested_data_type_ids()) {
    syncer::ModelType data_type =
        syncer::GetModelTypeFromSpecificsFieldNumber(field_number);
    if (!syncer::IsRealDataType(data_type)) {
      return false;
    }
    data_types.Put(data_type);
  }
  return data_types == expected_data_types;
}

MATCHER_P(InterestedDataTypesContain, expected_data_types, "") {
  syncer::ModelTypeSet data_types;
  for (const int field_number : arg.specifics()
                                    .device_info()
                                    .invalidation_fields()
                                    .interested_data_type_ids()) {
    syncer::ModelType data_type =
        syncer::GetModelTypeFromSpecificsFieldNumber(field_number);
    if (!syncer::IsRealDataType(data_type)) {
      return false;
    }
    data_types.Put(data_type);
  }
  return data_types.HasAll(expected_data_types);
}

MATCHER(HasInstanceIdToken, "") {
  return arg.specifics()
      .device_info()
      .invalidation_fields()
      .has_instance_id_token();
}

MATCHER_P(HasInstanceIdToken, expected_token, "") {
  return arg.specifics()
             .device_info()
             .invalidation_fields()
             .instance_id_token() == expected_token;
}

// This class helps to count the number of GU_TRIGGER events for the |type|
// since the object has been created.
class GetUpdatesTriggeredObserver : public fake_server::FakeServer::Observer {
 public:
  GetUpdatesTriggeredObserver(fake_server::FakeServer* fake_server,
                              syncer::ModelType type)
      : fake_server_(fake_server), type_(type) {
    fake_server_->AddObserver(this);
  }

  ~GetUpdatesTriggeredObserver() override {
    fake_server_->RemoveObserver(this);
  }

  void OnSuccessfulGetUpdates() override {
    sync_pb::ClientToServerMessage message;
    fake_server_->GetLastGetUpdatesMessage(&message);

    if (message.get_updates().get_updates_origin() !=
        sync_pb::SyncEnums::GU_TRIGGER) {
      return;
    }
    for (const sync_pb::DataTypeProgressMarker& progress_marker :
         message.get_updates().from_progress_marker()) {
      if (progress_marker.data_type_id() !=
          syncer::GetSpecificsFieldNumberFromModelType(type_)) {
        continue;
      }
      if (progress_marker.get_update_triggers().datatype_refresh_nudges() > 0) {
        num_nudged_get_updates_for_data_type_++;
      }
    }
  }

  size_t num_nudged_get_updates_for_data_type() const {
    return num_nudged_get_updates_for_data_type_;
  }

 private:
  const raw_ptr<fake_server::FakeServer> fake_server_;
  const syncer::ModelType type_;

  size_t num_nudged_get_updates_for_data_type_ = 0;
};

sync_pb::DeviceInfoSpecifics CreateDeviceInfoSpecifics(
    const std::string& cache_guid,
    const std::string& fcm_registration_token) {
  sync_pb::DeviceInfoSpecifics specifics;
  specifics.set_cache_guid(cache_guid);
  specifics.set_client_name("client name");
  specifics.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_LINUX);
  specifics.set_sync_user_agent("user agent");
  specifics.set_chrome_version("chrome version");
  specifics.set_signin_scoped_device_id("scoped device id");
  specifics.set_last_updated_timestamp(
      syncer::TimeToProtoTime(base::Time::Now()));
  specifics.mutable_invalidation_fields()->set_instance_id_token(
      fcm_registration_token);
  return specifics;
}

class SingleClientWithSyncSendInterestedDataTypesTest : public SyncTest {
 public:
  SingleClientWithSyncSendInterestedDataTypesTest() : SyncTest(SINGLE_CLIENT) {
    override_features_.InitWithFeatures(
        /*enabled_features=*/{switches::kSyncSendInterestedDataTypes},
        /*disabled_features=*/{
            switches::kUseSyncInvalidations,
            switches::kUseSyncInvalidationsForWalletAndOffer});
  }

  SingleClientWithSyncSendInterestedDataTypesTest(
      const SingleClientWithSyncSendInterestedDataTypesTest&) = delete;
  SingleClientWithSyncSendInterestedDataTypesTest& operator=(
      const SingleClientWithSyncSendInterestedDataTypesTest&) = delete;

  ~SingleClientWithSyncSendInterestedDataTypesTest() override = default;

 private:
  base::test::ScopedFeatureList override_features_;
};

IN_PROC_BROWSER_TEST_F(SingleClientWithSyncSendInterestedDataTypesTest,
                       SendInterestedDataTypesAsPartOfDeviceInfo) {
  ASSERT_TRUE(SetupSync());

  syncer::SyncInvalidationsService* sync_invalidations_service =
      SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0));
  ASSERT_THAT(sync_invalidations_service, NotNull());
  ASSERT_TRUE(sync_invalidations_service->GetInterestedDataTypes());
  const syncer::ModelTypeSet interested_data_types =
      *sync_invalidations_service->GetInterestedDataTypes();

  // Check that some "standard" data types are included.
  EXPECT_TRUE(
      interested_data_types.HasAll({syncer::NIGORI, syncer::BOOKMARKS}));
  // Wallet and Offer data types are excluded unless
  // kUseSyncInvalidationsForWalletAndOffer is also enabled.
  EXPECT_FALSE(interested_data_types.Has(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_FALSE(interested_data_types.Has(syncer::AUTOFILL_WALLET_OFFER));

  // The local device should eventually be committed to the server.
  // The InstanceID token should only be uploaded if kUseSyncInvalidations is
  // also enabled.
  EXPECT_TRUE(
      ServerDeviceInfoMatchChecker(
          GetFakeServer(),
          ElementsAre(AllOf(InterestedDataTypesAre(interested_data_types),
                            Not(HasInstanceIdToken()))))
          .Wait());
}

class SingleClientWithUseSyncInvalidationsTest : public SyncTest {
 public:
  SingleClientWithUseSyncInvalidationsTest() : SyncTest(SINGLE_CLIENT) {
    override_features_.InitWithFeatures(
        /*enabled_features=*/{switches::kSyncSendInterestedDataTypes,
                              switches::kUseSyncInvalidations},
        /*disabled_features=*/{
            switches::kUseSyncInvalidationsForWalletAndOffer});
  }

  SingleClientWithUseSyncInvalidationsTest(
      const SingleClientWithUseSyncInvalidationsTest&) = delete;
  SingleClientWithUseSyncInvalidationsTest& operator=(
      const SingleClientWithUseSyncInvalidationsTest&) = delete;

  ~SingleClientWithUseSyncInvalidationsTest() override = default;

  // Injects a test DeviceInfo entity to the fake server.
  void InjectDeviceInfoEntityToServer(
      const std::string& cache_guid,
      const std::string& fcm_registration_token) {
    sync_pb::EntitySpecifics specifics;
    *specifics.mutable_device_info() =
        CreateDeviceInfoSpecifics(cache_guid, fcm_registration_token);
    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"",
            /*client_tag=*/
            syncer::DeviceInfoUtil::SpecificsToTag(specifics.device_info()),
            specifics,
            /*creation_time=*/specifics.device_info().last_updated_timestamp(),
            /*last_modified_time=*/
            specifics.device_info().last_updated_timestamp()));
  }

  std::string GetLocalCacheGuid() {
    syncer::SyncTransportDataPrefs prefs(GetProfile(0)->GetPrefs());
    return prefs.GetCacheGuid();
  }

 private:
  base::test::ScopedFeatureList override_features_;
};

IN_PROC_BROWSER_TEST_F(SingleClientWithUseSyncInvalidationsTest,
                       SendInterestedDataTypesAndFCMTokenAsPartOfDeviceInfo) {
  ASSERT_TRUE(SetupSync());

  syncer::SyncInvalidationsService* sync_invalidations_service =
      SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0));
  ASSERT_THAT(sync_invalidations_service, NotNull());
  ASSERT_TRUE(sync_invalidations_service->GetInterestedDataTypes());
  ASSERT_TRUE(sync_invalidations_service->GetFCMRegistrationToken());
  const syncer::ModelTypeSet interested_data_types =
      *sync_invalidations_service->GetInterestedDataTypes();
  const std::string fcm_token =
      *sync_invalidations_service->GetFCMRegistrationToken();

  // Check that some "standard" data types are included.
  EXPECT_TRUE(
      interested_data_types.HasAll({syncer::NIGORI, syncer::BOOKMARKS}));
  // Wallet and Offer data types are excluded unless
  // kUseSyncInvalidationsForWalletAndOffer is also enabled.
  EXPECT_FALSE(interested_data_types.Has(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_FALSE(interested_data_types.Has(syncer::AUTOFILL_WALLET_OFFER));
  EXPECT_FALSE(fcm_token.empty());

  // The local device should eventually be committed to the server.
  EXPECT_TRUE(
      ServerDeviceInfoMatchChecker(
          GetFakeServer(),
          ElementsAre(AllOf(InterestedDataTypesAre(interested_data_types),
                            HasInstanceIdToken(fcm_token))))
          .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientWithUseSyncInvalidationsTest,
                       ShouldPopulateFCMRegistrationTokens) {
  const std::string kTitle = "title";
  const std::string kRemoteDeviceCacheGuid = "other_cache_guid";
  const std::string kRemoteFCMRegistrationToken = "other_fcm_token";

  // Simulate the case when the server already knows one other device.
  InjectDeviceInfoEntityToServer(kRemoteDeviceCacheGuid,
                                 kRemoteFCMRegistrationToken);
  ASSERT_TRUE(SetupSync());

  // Commit a new bookmark to check if the next commit message has FCM
  // registration tokens.
  AddFolder(0, GetBookmarkBarNode(0), 0, kTitle);
  ASSERT_TRUE(ServerBookmarksEqualityChecker(GetSyncService(0), GetFakeServer(),
                                             {{kTitle, GURL()}},
                                             /*cryptographer=*/nullptr)
                  .Wait());

  sync_pb::ClientToServerMessage message;
  GetFakeServer()->GetLastCommitMessage(&message);

  EXPECT_THAT(
      message.commit().config_params().devices_fcm_registration_tokens(),
      ElementsAre(kRemoteFCMRegistrationToken));
}

IN_PROC_BROWSER_TEST_F(SingleClientWithUseSyncInvalidationsTest,
                       PRE_ShouldNotSendAdditionalGetUpdates) {
  ASSERT_TRUE(SetupSync());
}

IN_PROC_BROWSER_TEST_F(SingleClientWithUseSyncInvalidationsTest,
                       ShouldNotSendAdditionalGetUpdates) {
  const std::vector<sync_pb::SyncEntity> server_device_infos_before =
      fake_server_->GetSyncEntitiesByModelType(syncer::DEVICE_INFO);

  // Check here for size only, cache GUID will be verified after SetupcClients()
  // call.
  ASSERT_THAT(server_device_infos_before, SizeIs(1));
  const int64_t last_updated_timestamp = server_device_infos_before.front()
                                             .specifics()
                                             .device_info()
                                             .last_updated_timestamp();

  GetUpdatesTriggeredObserver observer(GetFakeServer(),
                                       syncer::ModelType::AUTOFILL);
  ASSERT_TRUE(SetupClients());
  ASSERT_THAT(server_device_infos_before,
              ElementsAre(HasCacheGuid(GetLocalCacheGuid())));

  // Trigger DeviceInfo reupload once it has been initialized. This is mimics
  // the case when DeviceInfo is outdated on browser startup.
  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(0))
      ->GetDeviceInfoTracker()
      ->ForcePulseForTest();
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());

  // Wait until DeviceInfo is updated.
  ASSERT_TRUE(ServerDeviceInfoMatchChecker(
                  GetFakeServer(),
                  ElementsAre(HasBeenUpdatedAfter(last_updated_timestamp)))
                  .Wait());

  // Perform an additional sync cycle to be sure that there will be at least one
  // more GetUpdates request if it was triggered.
  const std::string kTitle1 = "Title 1";
  AddFolder(0, GetBookmarkBarNode(0), 0, kTitle1);
  ASSERT_TRUE(ServerBookmarksEqualityChecker(GetSyncService(0), GetFakeServer(),
                                             {{kTitle1, GURL()}},
                                             /*cryptographer=*/nullptr)
                  .Wait());

  const std::string kTitle2 = "Title 2";
  AddFolder(0, GetBookmarkBarNode(0), 0, kTitle2);
  ASSERT_TRUE(
      ServerBookmarksEqualityChecker(GetSyncService(0), GetFakeServer(),
                                     {{kTitle1, GURL()}, {kTitle2, GURL()}},
                                     /*cryptographer=*/nullptr)
          .Wait());

  // There will be one TriggerRefresh request in tests due to
  // ConfigurationRefresher. There shouldn't be any additional GU_TRIGGER
  // with nudge DeviceInfo data type.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On ChromeOS tests data types are configured twice and hence there are two
  // expected TriggerRefresh calls during initialization. It happens due to
  // SyncArcPackageHelper which eventually triggers reconfiguration.
  EXPECT_EQ(2u, observer.num_nudged_get_updates_for_data_type());
#else
  EXPECT_EQ(1u, observer.num_nudged_get_updates_for_data_type());
#endif
}

class SingleClientWithUseSyncInvalidationsForWalletAndOfferTest
    : public SyncTest {
 public:
  SingleClientWithUseSyncInvalidationsForWalletAndOfferTest()
      : SyncTest(SINGLE_CLIENT) {
    override_features_.InitWithFeatures(
        /*enabled_features=*/{switches::kSyncSendInterestedDataTypes,
                              switches::kUseSyncInvalidations,
                              switches::kUseSyncInvalidationsForWalletAndOffer},
        /*disabled_features=*/{});
  }

  SingleClientWithUseSyncInvalidationsForWalletAndOfferTest(
      const SingleClientWithUseSyncInvalidationsForWalletAndOfferTest&) =
      delete;
  SingleClientWithUseSyncInvalidationsForWalletAndOfferTest& operator=(
      const SingleClientWithUseSyncInvalidationsForWalletAndOfferTest&) =
      delete;

  ~SingleClientWithUseSyncInvalidationsForWalletAndOfferTest() override =
      default;

  void InjectSyncedBookmark() {
    fake_server::BookmarkEntityBuilder bookmark_builder =
        entity_builder_factory_.NewBookmarkEntityBuilder("My Bookmark");
    GetFakeServer()->InjectEntity(
        bookmark_builder.BuildBookmark(GURL(kSyncedBookmarkURL)));
  }

 private:
  base::test::ScopedFeatureList override_features_;
  fake_server::EntityBuilderFactory entity_builder_factory_;
};

IN_PROC_BROWSER_TEST_F(
    SingleClientWithUseSyncInvalidationsForWalletAndOfferTest,
    SendInterestedDataTypesAndFCMTokenAsPartOfDeviceInfo) {
  ASSERT_TRUE(SetupSync());

  syncer::SyncInvalidationsService* sync_invalidations_service =
      SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0));
  ASSERT_THAT(sync_invalidations_service, NotNull());
  ASSERT_TRUE(sync_invalidations_service->GetInterestedDataTypes());
  ASSERT_TRUE(sync_invalidations_service->GetFCMRegistrationToken());
  const syncer::ModelTypeSet interested_data_types =
      *sync_invalidations_service->GetInterestedDataTypes();
  const std::string fcm_token =
      *sync_invalidations_service->GetFCMRegistrationToken();

  // Check that some "standard" data types are included.
  EXPECT_TRUE(
      interested_data_types.HasAll({syncer::NIGORI, syncer::BOOKMARKS}));
  // Wallet data type should be included by default if
  // kUseSyncInvalidationsForWalletAndOffer is enabled.
  EXPECT_TRUE(interested_data_types.Has(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_FALSE(fcm_token.empty());

  // The local device should eventually be committed to the server.
  EXPECT_TRUE(
      ServerDeviceInfoMatchChecker(
          GetFakeServer(),
          ElementsAre(AllOf(InterestedDataTypesAre(interested_data_types),
                            HasInstanceIdToken(fcm_token))))
          .Wait());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientWithUseSyncInvalidationsForWalletAndOfferTest,
    EnableAndDisableADataType) {
  ASSERT_TRUE(SetupSync());

  // The local device should eventually be committed to the server. BOOKMARKS
  // should be included in interested types, since it's enabled by default.
  EXPECT_TRUE(ServerDeviceInfoMatchChecker(
                  GetFakeServer(),
                  ElementsAre(InterestedDataTypesContain(syncer::BOOKMARKS)))
                  .Wait());

  // Disable BOOKMARKS.
  ASSERT_TRUE(
      GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kBookmarks));
  // The local device should eventually be committed to the server. BOOKMARKS
  // should not be included in interested types, as it was disabled.
  EXPECT_TRUE(
      ServerDeviceInfoMatchChecker(
          GetFakeServer(),
          ElementsAre(Not(InterestedDataTypesContain(syncer::BOOKMARKS))))
          .Wait());

  // Create a bookmark on the server.
  InjectSyncedBookmark();
  // Enable BOOKMARKS again.
  ASSERT_TRUE(
      GetClient(0)->EnableSyncForType(syncer::UserSelectableType::kBookmarks));
  // The local device should eventually be committed to the server. BOOKMARKS
  // should now be included in interested types.
  EXPECT_TRUE(ServerDeviceInfoMatchChecker(
                  GetFakeServer(),
                  ElementsAre(InterestedDataTypesContain(syncer::BOOKMARKS)))
                  .Wait());
  // The bookmark should get synced now.
  EXPECT_TRUE(bookmarks_helper::GetBookmarkModel(0)->IsBookmarked(
      GURL(kSyncedBookmarkURL)));
}

// ChromeOS doesn't have the concept of sign-out.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

// On Lacros, signout is not supported with Mirror account consistency.
// TODO(https://crbug.com/1260291): Enable this test once signout is supported.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_SignoutAndSignin DISABLED_SignoutAndSignin
#else
#define MAYBE_SignoutAndSignin SignoutAndSignin
#endif
IN_PROC_BROWSER_TEST_F(
    SingleClientWithUseSyncInvalidationsForWalletAndOfferTest,
    MAYBE_SignoutAndSignin) {
  ASSERT_TRUE(SetupSync());

  // The local device should eventually be committed to the server. The FCM
  // token should be present in device info.
  ASSERT_TRUE(SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0))
                  ->GetFCMRegistrationToken());
  const std::string old_token =
      *SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0))
           ->GetFCMRegistrationToken();
  EXPECT_TRUE(ServerDeviceInfoMatchChecker(
                  GetFakeServer(), ElementsAre(HasInstanceIdToken(old_token)))
                  .Wait());

  // Sign out. The FCM token should be cleared.
  GetClient(0)->SignOutPrimaryAccount();
  ASSERT_TRUE(SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0))
                  ->GetFCMRegistrationToken());
  EXPECT_TRUE(SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0))
                  ->GetFCMRegistrationToken()
                  ->empty());

  // Sign in again.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0))
                  ->GetFCMRegistrationToken());
  const std::string new_token =
      *SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0))
           ->GetFCMRegistrationToken();
  EXPECT_NE(new_token, old_token);
  EXPECT_FALSE(new_token.empty());
  // New device info should eventually be committed to the server (but the old
  // device info will remain on the server). The FCM token should be present.
  EXPECT_TRUE(
      ServerDeviceInfoMatchChecker(
          GetFakeServer(), UnorderedElementsAre(HasInstanceIdToken(old_token),
                                                HasInstanceIdToken(new_token)))
          .Wait());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

class SingleClientSyncInvalidationsTestWithPreDisabledSendInterestedDataTypes
    : public SyncTest {
 public:
  SingleClientSyncInvalidationsTestWithPreDisabledSendInterestedDataTypes()
      : SyncTest(SINGLE_CLIENT) {
    features_override_.InitWithFeatureState(
        switches::kSyncSendInterestedDataTypes, !content::IsPreTest());
  }

  std::string GetLocalCacheGuid() {
    syncer::SyncTransportDataPrefs prefs(GetProfile(0)->GetPrefs());
    return prefs.GetCacheGuid();
  }

 private:
  base::test::ScopedFeatureList features_override_;
};

IN_PROC_BROWSER_TEST_F(
    SingleClientSyncInvalidationsTestWithPreDisabledSendInterestedDataTypes,
    PRE_ShouldResendDeviceInfoWithInterestedDataTypes) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(ServerDeviceInfoMatchChecker(
                  GetFakeServer(),
                  UnorderedElementsAre(HasCacheGuid(GetLocalCacheGuid())))
                  .Wait());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientSyncInvalidationsTestWithPreDisabledSendInterestedDataTypes,
    ShouldResendDeviceInfoWithInterestedDataTypes) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());

  EXPECT_TRUE(ServerDeviceInfoMatchChecker(
                  GetFakeServer(),
                  ElementsAre(InterestedDataTypesContain(syncer::NIGORI)))
                  .Wait());
}

}  // namespace
