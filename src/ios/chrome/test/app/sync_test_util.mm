// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/sync_test_util.h"

#include <set>
#include <string>

#include "base/bind.h"
#include "base/check.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/metrics/test/demographic_metrics_test_utils.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/engine_impl/loopback_server/loopback_server_entity.h"
#include "components/sync/nigori/nigori_test_utils.h"
#include "components/sync/test/fake_server/entity_builder_factory.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "components/sync/test/fake_server/fake_server_nigori_helper.h"
#include "components/sync/test/fake_server/fake_server_verifier.h"
#include "components/sync/test/fake_server/sessions_hierarchy.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/sync/device_info_sync_service_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

fake_server::FakeServer* gSyncFakeServer = nullptr;

NSString* const kSyncTestErrorDomain = @"SyncTestDomain";

// Overrides the network callback of the current ProfileSyncService with
// |create_http_post_provider_factory_cb|.
void OverrideSyncNetwork(const syncer::CreateHttpPostProviderFactory&
                             create_http_post_provider_factory_cb) {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  DCHECK(browser_state);
  syncer::ProfileSyncService* service =
      ProfileSyncServiceFactory::GetAsProfileSyncServiceForBrowserState(
          browser_state);
  service->OverrideNetworkForTest(create_http_post_provider_factory_cb);
}

// Returns a bookmark server entity based on |title| and |url|.
std::unique_ptr<syncer::LoopbackServerEntity> CreateBookmarkServerEntity(
    const std::string& title,
    const GURL& url) {
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  return bookmark_builder.BuildBookmark(url);
}

}  // namespace

namespace chrome_test_util {

void SetUpFakeSyncServer() {
  DCHECK(!gSyncFakeServer);
  gSyncFakeServer = new fake_server::FakeServer();
  OverrideSyncNetwork(fake_server::CreateFakeServerHttpPostProviderFactory(
      gSyncFakeServer->AsWeakPtr()));
}

void TearDownFakeSyncServer() {
  DCHECK(gSyncFakeServer);
  delete gSyncFakeServer;
  gSyncFakeServer = nullptr;
  OverrideSyncNetwork(syncer::CreateHttpPostProviderFactory());
}

void StartSync() {
  DCHECK(!IsSyncInitialized());
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  SyncSetupService* sync_setup_service =
      SyncSetupServiceFactory::GetForBrowserState(browser_state);
  sync_setup_service->SetSyncEnabled(true);
}

void StopSync() {
  DCHECK(IsSyncInitialized());
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  SyncSetupService* sync_setup_service =
      SyncSetupServiceFactory::GetForBrowserState(browser_state);
  sync_setup_service->SetSyncEnabled(false);
}

void TriggerSyncCycle(syncer::ModelType type) {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForBrowserState(browser_state);
  sync_service->TriggerRefresh({type});
}

void ClearSyncServerData() {
  DCHECK(gSyncFakeServer);
  gSyncFakeServer->ClearServerData();
}

int GetNumberOfSyncEntities(syncer::ModelType type) {
  std::unique_ptr<base::DictionaryValue> entities =
      gSyncFakeServer->GetEntitiesAsDictionaryValue();

  std::string model_type_string = ModelTypeToString(type);
  base::ListValue* entity_list = NULL;
  if (!entities->GetList(model_type_string, &entity_list)) {
    return 0;
  }
  return entity_list->GetSize();
}

BOOL VerifyNumberOfSyncEntitiesWithName(syncer::ModelType type,
                                        std::string name,
                                        size_t count,
                                        NSError** error) {
  DCHECK(gSyncFakeServer);
  fake_server::FakeServerVerifier verifier(gSyncFakeServer);
  testing::AssertionResult result =
      verifier.VerifyEntityCountByTypeAndName(count, type, name);
  if (result != testing::AssertionSuccess() && error != nil) {
    NSDictionary* errorInfo = @{
      NSLocalizedDescriptionKey : base::SysUTF8ToNSString(result.message())
    };
    *error = [NSError errorWithDomain:kSyncTestErrorDomain
                                 code:0
                             userInfo:errorInfo];
    return NO;
  }
  return result == testing::AssertionSuccess();
}

void AddBookmarkToFakeSyncServer(std::string url, std::string title) {
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  gSyncFakeServer->InjectEntity(bookmark_builder.BuildBookmark(GURL(url)));
}

void AddLegacyBookmarkToFakeSyncServer(std::string url,
                                       std::string title,
                                       std::string originator_client_item_id) {
  DCHECK(!base::IsValidGUID(originator_client_item_id));
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(
          title, std::move(originator_client_item_id));
  gSyncFakeServer->InjectEntity(
      bookmark_builder.BuildBookmark(GURL(url), /*is_legacy=*/true));
}

bool IsSyncInitialized() {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  DCHECK(browser_state);
  syncer::SyncService* syncService =
      ProfileSyncServiceFactory::GetForBrowserState(browser_state);
  return syncService->IsEngineInitialized();
}

std::string GetSyncCacheGuid() {
  DCHECK(IsSyncInitialized());
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  syncer::DeviceInfoSyncService* service =
      DeviceInfoSyncServiceFactory::GetForBrowserState(browser_state);
  const syncer::LocalDeviceInfoProvider* info_provider =
      service->GetLocalDeviceInfoProvider();
  return info_provider->GetLocalDeviceInfo()->guid();
}

void AddUserDemographicsToSyncServer(
    int birth_year,
    metrics::UserDemographicsProto::Gender gender) {
  metrics::test::AddUserBirthYearAndGenderToSyncServer(
      gSyncFakeServer->AsWeakPtr(), birth_year, gender);
}

void AddAutofillProfileToFakeSyncServer(std::string guid,
                                        std::string full_name) {
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::AutofillProfileSpecifics* autofill_profile =
      entity_specifics.mutable_autofill_profile();
  autofill_profile->add_name_full(full_name);
  autofill_profile->set_guid(guid);

  std::unique_ptr<syncer::LoopbackServerEntity> entity =
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/guid, /*client_tag=*/guid, entity_specifics,
          12345, 12345);
  gSyncFakeServer->InjectEntity(std::move(entity));
}

void DeleteAutofillProfileFromFakeSyncServer(std::string guid) {
  std::vector<sync_pb::SyncEntity> autofill_profiles =
      gSyncFakeServer->GetSyncEntitiesByModelType(syncer::AUTOFILL_PROFILE);
  std::string entity_id;
  std::string client_tag_hash;
  for (const sync_pb::SyncEntity& autofill_profile : autofill_profiles) {
    if (autofill_profile.specifics().autofill_profile().guid() == guid) {
      entity_id = autofill_profile.id_string();
      client_tag_hash = autofill_profile.client_defined_unique_tag();
      break;
    }
  }
  // Delete the entity if it exists.
  if (!entity_id.empty()) {
    std::unique_ptr<syncer::LoopbackServerEntity> entity;
    entity = syncer::PersistentTombstoneEntity::CreateNew(entity_id,
                                                          client_tag_hash);
    gSyncFakeServer->InjectEntity(std::move(entity));
  }
}

bool IsAutofillProfilePresent(std::string guid, std::string full_name) {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  autofill::PersonalDataManager* personal_data_manager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(browser_state);
  autofill::AutofillProfile* autofill_profile =
      personal_data_manager->GetProfileByGUID(guid);

  if (autofill_profile) {
    std::string actual_full_name =
        base::UTF16ToUTF8(autofill_profile->GetRawInfo(autofill::NAME_FULL));
    return actual_full_name == full_name;
  }
  return false;
}

void ClearAutofillProfile(std::string guid) {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  autofill::PersonalDataManager* personal_data_manager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(browser_state);
  personal_data_manager->RemoveByGUID(guid);
}

BOOL VerifySessionsOnSyncServer(const std::multiset<std::string>& expected_urls,
                                NSError** error) {
  DCHECK(gSyncFakeServer);
  fake_server::SessionsHierarchy expected_sessions;
  expected_sessions.AddWindow(expected_urls);
  fake_server::FakeServerVerifier verifier(gSyncFakeServer);
  testing::AssertionResult result = verifier.VerifySessions(expected_sessions);
  if (result != testing::AssertionSuccess() && error != nil) {
    NSDictionary* errorInfo = @{
      NSLocalizedDescriptionKey : base::SysUTF8ToNSString(result.message())
    };
    *error = [NSError errorWithDomain:kSyncTestErrorDomain
                                 code:0
                             userInfo:errorInfo];
    return NO;
  }
  return result == testing::AssertionSuccess();
}

void AddTypedURLToClient(const GURL& url) {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  history::HistoryService* historyService =
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);

  historyService->AddPage(url, base::Time::Now(), nullptr, 1, GURL(),
                          history::RedirectList(), ui::PAGE_TRANSITION_TYPED,
                          history::SOURCE_BROWSED, false);
}

void AddTypedURLToFakeSyncServer(const std::string& url) {
  sync_pb::EntitySpecifics entitySpecifics;
  sync_pb::TypedUrlSpecifics* typedUrl = entitySpecifics.mutable_typed_url();
  typedUrl->set_url(url);
  typedUrl->set_title(url);
  typedUrl->add_visits(base::Time::Max().ToInternalValue());
  typedUrl->add_visit_transitions(sync_pb::SyncEnums::TYPED);

  std::unique_ptr<syncer::LoopbackServerEntity> entity =
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/std::string(), /*client_tag=*/url,
          entitySpecifics, 12345, 12345);
  gSyncFakeServer->InjectEntity(std::move(entity));
}

BOOL IsTypedUrlPresentOnClient(const GURL& url,
                               BOOL expect_present,
                               NSError** error) {
  // Call the history service.
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);

  const GURL block_safe_url(url);
  std::set<GURL> origins;
  origins.insert(block_safe_url);

  __block bool history_service_callback_called = false;
  __block int count = 0;
  history_service->GetCountsAndLastVisitForOriginsForTesting(
      origins, base::BindOnce(^(history::OriginCountAndLastVisitMap result) {
        auto iter = result.find(block_safe_url);
        if (iter != result.end())
          count = iter->second.first;
        history_service_callback_called = true;
      }));

  NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:4.0];
  while (!history_service_callback_called &&
         [[NSDate date] compare:deadline] != NSOrderedDescending) {
    base::test::ios::SpinRunLoopWithMaxDelay(
        base::TimeDelta::FromSecondsD(0.1));
  }

  NSString* error_message = nil;
  if (!history_service_callback_called) {
    error_message = @"History::GetCountsAndLastVisitForOrigins callback never "
                     "called, app will probably crash later.";
  } else if (count == 0 && expect_present) {
    error_message = @"Typed URL isn't found in HistoryService.";
  } else if (count > 0 && !expect_present) {
    error_message = @"Typed URL isn't supposed to be in HistoryService.";
  }

  if (error_message != nil && error != nil) {
    NSDictionary* error_info = @{NSLocalizedDescriptionKey : error_message};
    *error = [NSError errorWithDomain:kSyncTestErrorDomain
                                 code:0
                             userInfo:error_info];
    return NO;
  }
  return error_message == nil;
}

void DeleteTypedUrlFromClient(const GURL& url) {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);

  history_service->DeleteURLs({url});
}

void DeleteTypedUrlFromFakeSyncServer(std::string url) {
  std::vector<sync_pb::SyncEntity> typed_urls =
      gSyncFakeServer->GetSyncEntitiesByModelType(syncer::TYPED_URLS);
  std::string entity_id;
  for (const sync_pb::SyncEntity& typed_url : typed_urls) {
    if (typed_url.specifics().typed_url().url() == url) {
      entity_id = typed_url.id_string();
      break;
    }
  }
  if (!entity_id.empty()) {
    std::unique_ptr<syncer::LoopbackServerEntity> entity;
    entity =
        syncer::PersistentTombstoneEntity::CreateNew(entity_id, std::string());
    gSyncFakeServer->InjectEntity(std::move(entity));
  }
}

void AddBookmarkWithSyncPassphrase(const std::string& sync_passphrase) {
  syncer::KeyParamsForTesting key_params = {
      syncer::KeyDerivationParams::CreateForPbkdf2(), sync_passphrase};
  std::unique_ptr<syncer::LoopbackServerEntity> server_entity =
      CreateBookmarkServerEntity("PBKDF2-encrypted bookmark",
                                 GURL("http://example.com/doesnt-matter"));
  server_entity->SetSpecifics(GetEncryptedBookmarkEntitySpecifics(
      server_entity->GetSpecifics().bookmark(), key_params));
  gSyncFakeServer->InjectEntity(std::move(server_entity));
  fake_server::SetNigoriInFakeServer(CreateCustomPassphraseNigori(key_params),
                                     gSyncFakeServer);
}

}  // namespace chrome_test_util
