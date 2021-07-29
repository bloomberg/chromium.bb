// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/storage/settings_sync_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/test/model/fake_sync_change_processor.h"
#include "components/sync/test/model/sync_change_processor_wrapper_for_test.h"
#include "components/sync/test/model/sync_error_factory_mock.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/storage_area_namespace.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/value_store/settings_namespace.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

using testing::_;
using testing::Mock;
using testing::NiceMock;
using testing::Return;

namespace {

// TODO(kalman): test both EXTENSION_SETTINGS and APP_SETTINGS.
const syncer::ModelType kModelType = syncer::EXTENSION_SETTINGS;

// The managed_storage extension has a key defined in its manifest, so that
// its extension ID is well-known and the policy system can push policies for
// the extension.
const char kManagedStorageExtensionId[] = "kjmkgkdkpedkejedfhmfcenooemhbpbo";

class MockSchemaRegistryObserver : public policy::SchemaRegistry::Observer {
 public:
  MockSchemaRegistryObserver() {}
  ~MockSchemaRegistryObserver() override {}

  MOCK_METHOD1(OnSchemaRegistryUpdated, void(bool));
};

}  // namespace

class ExtensionSettingsApiTest : public ExtensionApiTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();

    ON_CALL(policy_provider_, IsInitializationComplete(_))
        .WillByDefault(Return(true));
    ON_CALL(policy_provider_, IsFirstPolicyLoadComplete(_))
        .WillByDefault(Return(true));
    policy_provider_.SetAutoRefresh();
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void ReplyWhenSatisfied(StorageAreaNamespace storage_area,
                          const std::string& normal_action,
                          const std::string& incognito_action) {
    MaybeLoadAndReplyWhenSatisfied(storage_area, normal_action,
                                   incognito_action, nullptr, false);
  }

  const Extension* LoadAndReplyWhenSatisfied(
      StorageAreaNamespace storage_area,
      const std::string& normal_action,
      const std::string& incognito_action,
      const std::string& extension_dir) {
    return MaybeLoadAndReplyWhenSatisfied(
        storage_area, normal_action, incognito_action, &extension_dir, false);
  }

  void FinalReplyWhenSatisfied(StorageAreaNamespace storage_area,
                               const std::string& normal_action,
                               const std::string& incognito_action) {
    MaybeLoadAndReplyWhenSatisfied(storage_area, normal_action,
                                   incognito_action, nullptr, true);
  }

  static void InitSyncOnBackgroundSequence(
      base::OnceCallback<base::WeakPtr<syncer::SyncableService>()>
          syncable_service_provider,
      syncer::SyncChangeProcessor* sync_processor) {
    DCHECK(GetBackendTaskRunner()->RunsTasksInCurrentSequence());

    base::WeakPtr<syncer::SyncableService> syncable_service =
        std::move(syncable_service_provider).Run();
    DCHECK(syncable_service.get());
    EXPECT_FALSE(
        syncable_service
            ->MergeDataAndStartSyncing(
                kModelType, syncer::SyncDataList(),
                std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
                    sync_processor),
                std::make_unique<NiceMock<syncer::SyncErrorFactoryMock>>())
            .has_value());
  }

  void InitSync(syncer::SyncChangeProcessor* sync_processor) {
    base::RunLoop().RunUntilIdle();

    base::RunLoop loop;
    GetBackendTaskRunner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&InitSyncOnBackgroundSequence,
                       settings_sync_util::GetSyncableServiceProvider(
                           profile(), kModelType),
                       sync_processor),
        loop.QuitClosure());
    loop.Run();
  }

  static void SendChangesOnBackgroundSequence(
      base::OnceCallback<base::WeakPtr<syncer::SyncableService>()>
          syncable_service_provider,
      const syncer::SyncChangeList& change_list) {
    DCHECK(GetBackendTaskRunner()->RunsTasksInCurrentSequence());

    base::WeakPtr<syncer::SyncableService> syncable_service =
        std::move(syncable_service_provider).Run();
    DCHECK(syncable_service.get());
    EXPECT_FALSE(syncable_service->ProcessSyncChanges(FROM_HERE, change_list)
                     .has_value());
  }

  void SendChanges(const syncer::SyncChangeList& change_list) {
    base::RunLoop().RunUntilIdle();

    base::RunLoop loop;
    GetBackendTaskRunner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&SendChangesOnBackgroundSequence,
                       settings_sync_util::GetSyncableServiceProvider(
                           profile(), kModelType),
                       change_list),
        loop.QuitClosure());
    loop.Run();
  }

  void SetPolicies(const base::DictionaryValue& policies) {
    std::unique_ptr<policy::PolicyBundle> bundle(new policy::PolicyBundle());
    policy::PolicyMap& policy_map = bundle->Get(policy::PolicyNamespace(
        policy::POLICY_DOMAIN_EXTENSIONS, kManagedStorageExtensionId));
    policy_map.LoadFrom(&policies, policy::POLICY_LEVEL_MANDATORY,
                        policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD);
    policy_provider_.UpdatePolicy(std::move(bundle));
  }

 private:
  const Extension* MaybeLoadAndReplyWhenSatisfied(
      StorageAreaNamespace storage_area,
      const std::string& normal_action,
      const std::string& incognito_action,
      // May be NULL to imply not loading the extension.
      const std::string* extension_dir,
      bool is_final_action) {
    ExtensionTestMessageListener listener("waiting", true);
    ExtensionTestMessageListener listener_incognito("waiting_incognito", true);

    // Only load the extension after the listeners have been set up, to avoid
    // initialisation race conditions.
    const Extension* extension = NULL;
    if (extension_dir) {
      extension = LoadExtension(
          test_data_dir_.AppendASCII("settings").AppendASCII(*extension_dir),
          {.allow_in_incognito = true});
      EXPECT_TRUE(extension);
    }

    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_TRUE(listener_incognito.WaitUntilSatisfied());

    listener.Reply(CreateMessage(storage_area, normal_action, is_final_action));
    listener_incognito.Reply(
        CreateMessage(storage_area, incognito_action, is_final_action));
    return extension;
  }

  std::string CreateMessage(StorageAreaNamespace storage_area,
                            const std::string& action,
                            bool is_final_action) {
    base::DictionaryValue message;
    message.SetString("namespace", StorageAreaToString(storage_area));
    message.SetString("action", action);
    message.SetBoolean("isFinalAction", is_final_action);
    std::string message_json;
    base::JSONWriter::Write(message, &message_json);
    return message_json;
  }

  void SendChangesToSyncableService(
      const syncer::SyncChangeList& change_list,
      syncer::SyncableService* settings_service) {
  }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

// A specialization of ExtensionSettingsApiTest that pretends it's running
// on version_info::Channel::UNKNOWN.
class ExtensionSettingsTrunkApiTest : public ExtensionSettingsApiTest {
 public:
  ExtensionSettingsTrunkApiTest() = default;
  ~ExtensionSettingsTrunkApiTest() override = default;
  ExtensionSettingsTrunkApiTest(const ExtensionSettingsTrunkApiTest& other) =
      delete;
  ExtensionSettingsTrunkApiTest& operator=(
      const ExtensionSettingsTrunkApiTest& other) = delete;

 private:
  // TODO(crbug.com/1185226): Remove unknown channel when chrome.storage.session
  // is released in stable.
  ScopedCurrentChannel current_channel_{version_info::Channel::UNKNOWN};
};

// A specialization of ExtensionSettingsApiTest that pretends it's running
// on version_info::Channel::DEV.
class ExtensionSettingsDevApiTest : public ExtensionSettingsApiTest {
 public:
  ExtensionSettingsDevApiTest() = default;
  ~ExtensionSettingsDevApiTest() override = default;
  ExtensionSettingsDevApiTest(const ExtensionSettingsDevApiTest& other) =
      delete;
  ExtensionSettingsDevApiTest& operator=(
      const ExtensionSettingsDevApiTest& other) = delete;

 private:
  // TODO(crbug.com/1185226): Remove dev channel when chrome.storage.session
  // is released in stable.
  ScopedCurrentChannel current_channel_{version_info::Channel::DEV};
};

// TODO(crbug.com/1185226): Remove test when chrome.storage.session
// is released in stable.
IN_PROC_BROWSER_TEST_F(ExtensionSettingsDevApiTest,
                       SessionInUnsupportedChannel) {
  constexpr char kManifest[] =
      R"({
           "name": "Unsupported channel for session",
           "manifest_version": 3,
           "version": "0.1",
           "background": { "service_worker": "worker.js" },
           "permissions": ["storage"]
         })";

  constexpr char kWorker[] =
      R"(chrome.test.runTests([
          function unsupported() {
            chrome.test.assertEq(undefined, chrome.storage.session);
            chrome.test.assertTrue(!!chrome.storage.local);
            chrome.test.succeed();
          },
        ]);)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kWorker);

  ResultCatcher catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// TODO(crbug.com/1185226): Change parent class to `ExtensionSettingsApiTest`
// when chrome.storage.session is released in stable.
IN_PROC_BROWSER_TEST_F(ExtensionSettingsTrunkApiTest, SimpleTest) {
  ASSERT_TRUE(RunExtensionTest("settings/simple_test")) << message_;
}

// Structure of this test taken from IncognitoSplitMode.
// Note that only split-mode incognito is tested, because spanning mode
// incognito looks the same as normal mode when the only API activity comes
// from background pages.
// TODO(crbug.com/1185226): Change parent class to `ExtensionSettingsApiTest`
// when chrome.storage.session is released in stable.
IN_PROC_BROWSER_TEST_F(ExtensionSettingsTrunkApiTest, SplitModeIncognito) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher;
  ResultCatcher catcher_incognito;
  catcher.RestrictToBrowserContext(browser()->profile());
  catcher_incognito.RestrictToBrowserContext(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  // Sync, local and managed follow the same storage flow (RunWithStorage),
  // whereas session follows a separate flow (RunWithSession). For the purpose
  // of this test we can just test sync and session.
  StorageAreaNamespace storage_areas[2] = {StorageAreaNamespace::kSync,
                                           StorageAreaNamespace::kSession};
  LoadAndReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertEmpty",
                            "assertEmpty", "split_incognito");
  for (const StorageAreaNamespace& storage_area : storage_areas) {
    ReplyWhenSatisfied(storage_area, "assertEmpty", "assertEmpty");
    ReplyWhenSatisfied(storage_area, "noop", "setFoo");
    ReplyWhenSatisfied(storage_area, "assertFoo", "assertFoo");
    ReplyWhenSatisfied(storage_area, "clear", "noop");
    ReplyWhenSatisfied(storage_area, "assertEmpty", "assertEmpty");
    ReplyWhenSatisfied(storage_area, "setFoo", "noop");
    ReplyWhenSatisfied(storage_area, "assertFoo", "assertFoo");
    ReplyWhenSatisfied(storage_area, "noop", "removeFoo");
    ReplyWhenSatisfied(storage_area, "assertEmpty", "assertEmpty");
  }
  FinalReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertEmpty",
                          "assertEmpty");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

// TODO(crbug.com/1185226): Change parent class to `ExtensionSettingsApiTest`
// when chrome.storage.session is released in stable.
// Flaky across multiple platforms: https://crbug.com/1216449.
IN_PROC_BROWSER_TEST_F(ExtensionSettingsTrunkApiTest,
                       DISABLED_OnChangedNotificationsBetweenBackgroundPages) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher;
  ResultCatcher catcher_incognito;
  catcher.RestrictToBrowserContext(browser()->profile());
  catcher_incognito.RestrictToBrowserContext(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  StorageAreaNamespace storage_areas[2] = {StorageAreaNamespace::kSync,
                                           StorageAreaNamespace::kSession};
  LoadAndReplyWhenSatisfied(StorageAreaNamespace::kSync,
                            "assertNoNotifications", "assertNoNotifications",
                            "split_incognito");
  for (const StorageAreaNamespace& storage_area : storage_areas) {
    ReplyWhenSatisfied(storage_area, "assertNoNotifications",
                       "assertNoNotifications");
    ReplyWhenSatisfied(storage_area, "noop", "setFoo");
    ReplyWhenSatisfied(storage_area, "assertAddFooNotification",
                       "assertAddFooNotification");
    ReplyWhenSatisfied(storage_area, "clearNotifications",
                       "clearNotifications");
    ReplyWhenSatisfied(storage_area, "removeFoo", "noop");
    ReplyWhenSatisfied(storage_area, "assertDeleteFooNotification",
                       "assertDeleteFooNotification");
  }
  FinalReplyWhenSatisfied(StorageAreaNamespace::kSession,
                          "assertDeleteFooNotification",
                          "assertDeleteFooNotification");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

// TODO(crbug.com/1185226): Change parent class to `ExtensionSettingsApiTest`
// when chrome.storage.session is released in stable.
IN_PROC_BROWSER_TEST_F(ExtensionSettingsTrunkApiTest,
                       SyncLocalAndSessionAreasAreSeparate) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher;
  ResultCatcher catcher_incognito;
  catcher.RestrictToBrowserContext(browser()->profile());
  catcher_incognito.RestrictToBrowserContext(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  LoadAndReplyWhenSatisfied(StorageAreaNamespace::kSync,
                            "assertNoNotifications", "assertNoNotifications",
                            "split_incognito");

  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "noop", "setFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertAddFooNotification",
                     "assertAddFooNotification");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertEmpty",
                     "assertEmpty");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertNoNotifications",
                     "assertNoNotifications");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertEmpty",
                     "assertEmpty");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertNoNotifications",
                     "assertNoNotifications");

  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "clearNotifications",
                     "clearNotifications");

  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "setFoo", "noop");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertAddFooNotification",
                     "assertAddFooNotification");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertNoNotifications",
                     "assertNoNotifications");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertEmpty",
                     "assertEmpty");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertNoNotifications",
                     "assertNoNotifications");

  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "clearNotifications",
                     "clearNotifications");

  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "setFoo", "noop");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertAddFooNotification",
                     "assertAddFooNotification");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertNoNotifications",
                     "assertNoNotifications");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertNoNotifications",
                     "assertNoNotifications");

  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "clearNotifications",
                     "clearNotifications");

  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "noop", "removeFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertEmpty",
                     "assertEmpty");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal,
                     "assertDeleteFooNotification",
                     "assertDeleteFooNotification");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertNoNotifications",
                     "assertNoNotifications");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertNoNotifications",
                     "assertNoNotifications");

  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "clearNotifications",
                     "clearNotifications");

  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "removeFoo", "noop");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertEmpty", "assertEmpty");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertDeleteFooNotification",
                     "assertDeleteFooNotification");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertEmpty",
                     "assertEmpty");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertNoNotifications",
                     "assertNoNotifications");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertNoNotifications",
                     "assertNoNotifications");

  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "clearNotifications",
                     "clearNotifications");

  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "removeFoo", "noop");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertEmpty",
                     "assertEmpty");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession,
                     "assertDeleteFooNotification",
                     "assertDeleteFooNotification");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertEmpty", "assertEmpty");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertNoNotifications",
                     "assertNoNotifications");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertEmpty",
                     "assertEmpty");
  FinalReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertNoNotifications",
                          "assertNoNotifications");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

// https://crbug.com/1216450: Flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest,
                       DISABLED_OnChangedNotificationsFromSync) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher, catcher_incognito;
  catcher.RestrictToBrowserContext(browser()->profile());
  catcher_incognito.RestrictToBrowserContext(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  const Extension* extension = LoadAndReplyWhenSatisfied(
      StorageAreaNamespace::kSync, "assertNoNotifications",
      "assertNoNotifications", "split_incognito");
  const std::string& extension_id = extension->id();

  syncer::FakeSyncChangeProcessor sync_processor;
  InitSync(&sync_processor);

  // Set "foo" to "bar" via sync.
  syncer::SyncChangeList sync_changes;
  base::Value bar("bar");
  sync_changes.push_back(settings_sync_util::CreateAdd(
      extension_id, "foo", bar, kModelType));
  SendChanges(sync_changes);

  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertAddFooNotification",
                     "assertAddFooNotification");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "clearNotifications",
                     "clearNotifications");

  // Remove "foo" via sync.
  sync_changes.clear();
  sync_changes.push_back(settings_sync_util::CreateDelete(
      extension_id, "foo", kModelType));
  SendChanges(sync_changes);

  FinalReplyWhenSatisfied(StorageAreaNamespace::kSync,
                          "assertDeleteFooNotification",
                          "assertDeleteFooNotification");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

// TODO: boring test, already done in the unit tests.  What we really should be
// be testing is that the areas don't overlap.
IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest,
                       OnChangedNotificationsFromSyncNotSentToLocal) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher, catcher_incognito;
  catcher.RestrictToBrowserContext(browser()->profile());
  catcher_incognito.RestrictToBrowserContext(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  const Extension* extension = LoadAndReplyWhenSatisfied(
      StorageAreaNamespace::kLocal, "assertNoNotifications",
      "assertNoNotifications", "split_incognito");
  const std::string& extension_id = extension->id();

  syncer::FakeSyncChangeProcessor sync_processor;
  InitSync(&sync_processor);

  // Set "foo" to "bar" via sync.
  syncer::SyncChangeList sync_changes;
  base::Value bar("bar");
  sync_changes.push_back(settings_sync_util::CreateAdd(
      extension_id, "foo", bar, kModelType));
  SendChanges(sync_changes);

  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertNoNotifications",
                     "assertNoNotifications");

  // Remove "foo" via sync.
  sync_changes.clear();
  sync_changes.push_back(settings_sync_util::CreateDelete(
      extension_id, "foo", kModelType));
  SendChanges(sync_changes);

  FinalReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertNoNotifications",
                          "assertNoNotifications");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, IsStorageEnabled) {
  StorageFrontend* frontend = StorageFrontend::Get(browser()->profile());
  EXPECT_TRUE(frontend->IsStorageEnabled(settings_namespace::LOCAL));
  EXPECT_TRUE(frontend->IsStorageEnabled(settings_namespace::SYNC));

  EXPECT_TRUE(frontend->IsStorageEnabled(settings_namespace::MANAGED));
}

// Bulk disabled as part of arm64 bot stabilization: https://crbug.com/1154345
// TODO(crbug.com/1177118) Re-enable test
IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, DISABLED_ExtensionsSchemas) {
  // Verifies that the Schemas for the extensions domain are created on startup.
  Profile* profile = browser()->profile();
  ExtensionSystem* extension_system = ExtensionSystem::Get(profile);
  if (!extension_system->ready().is_signaled()) {
    // Wait until the extension system is ready.
    base::RunLoop run_loop;
    extension_system->ready().Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
    ASSERT_TRUE(extension_system->ready().is_signaled());
  }

  // This test starts without any test extensions installed.
  EXPECT_FALSE(GetSingleLoadedExtension());
  message_.clear();

  policy::SchemaRegistry* registry =
      profile->GetPolicySchemaRegistryService()->registry();
  ASSERT_TRUE(registry);
  EXPECT_FALSE(registry->schema_map()->GetSchema(policy::PolicyNamespace(
      policy::POLICY_DOMAIN_EXTENSIONS, kManagedStorageExtensionId)));

  NiceMock<MockSchemaRegistryObserver> observer;
  registry->AddObserver(&observer);

  // Install a managed extension.
  EXPECT_CALL(observer, OnSchemaRegistryUpdated(true));
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("settings/managed_storage"));
  ASSERT_TRUE(extension);
  Mock::VerifyAndClearExpectations(&observer);
  registry->RemoveObserver(&observer);

  // Verify that its schema has been published, and verify its contents.
  const policy::Schema* schema =
      registry->schema_map()->GetSchema(policy::PolicyNamespace(
          policy::POLICY_DOMAIN_EXTENSIONS, kManagedStorageExtensionId));
  ASSERT_TRUE(schema);

  ASSERT_TRUE(schema->valid());
  ASSERT_EQ(base::Value::Type::DICTIONARY, schema->type());
  ASSERT_TRUE(schema->GetKnownProperty("string-policy").valid());
  EXPECT_EQ(base::Value::Type::STRING,
            schema->GetKnownProperty("string-policy").type());
  ASSERT_TRUE(schema->GetKnownProperty("string-enum-policy").valid());
  EXPECT_EQ(base::Value::Type::STRING,
            schema->GetKnownProperty("string-enum-policy").type());
  ASSERT_TRUE(schema->GetKnownProperty("int-policy").valid());
  EXPECT_EQ(base::Value::Type::INTEGER,
            schema->GetKnownProperty("int-policy").type());
  ASSERT_TRUE(schema->GetKnownProperty("int-enum-policy").valid());
  EXPECT_EQ(base::Value::Type::INTEGER,
            schema->GetKnownProperty("int-enum-policy").type());
  ASSERT_TRUE(schema->GetKnownProperty("double-policy").valid());
  EXPECT_EQ(base::Value::Type::DOUBLE,
            schema->GetKnownProperty("double-policy").type());
  ASSERT_TRUE(schema->GetKnownProperty("boolean-policy").valid());
  EXPECT_EQ(base::Value::Type::BOOLEAN,
            schema->GetKnownProperty("boolean-policy").type());

  policy::Schema list = schema->GetKnownProperty("list-policy");
  ASSERT_TRUE(list.valid());
  ASSERT_EQ(base::Value::Type::LIST, list.type());
  ASSERT_TRUE(list.GetItems().valid());
  EXPECT_EQ(base::Value::Type::STRING, list.GetItems().type());

  policy::Schema dict = schema->GetKnownProperty("dict-policy");
  ASSERT_TRUE(dict.valid());
  ASSERT_EQ(base::Value::Type::DICTIONARY, dict.type());
  list = dict.GetKnownProperty("list");
  ASSERT_TRUE(list.valid());
  ASSERT_EQ(base::Value::Type::LIST, list.type());
  dict = list.GetItems();
  ASSERT_TRUE(dict.valid());
  ASSERT_EQ(base::Value::Type::DICTIONARY, dict.type());
  ASSERT_TRUE(dict.GetProperty("anything").valid());
  EXPECT_EQ(base::Value::Type::INTEGER, dict.GetProperty("anything").type());
}

// Bulk disabled as part of arm64 bot stabilization: https://crbug.com/1154345
// TODO(crbug.com/1177118) Re-enable test
IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, DISABLED_ManagedStorage) {
  // Set policies for the test extension.
  std::unique_ptr<base::DictionaryValue> policy =
      extensions::DictionaryBuilder()
          .Set("string-policy", "value")
          .Set("string-enum-policy", "value-1")
          .Set("another-string-policy", 123)  // Test invalid policy value.
          .Set("int-policy", -123)
          .Set("int-enum-policy", 1)
          .Set("double-policy", 456e7)
          .Set("boolean-policy", true)
          .Set("list-policy", extensions::ListBuilder()
                                  .Append("one")
                                  .Append("two")
                                  .Append("three")
                                  .Build())
          .Set("dict-policy",
               extensions::DictionaryBuilder()
                   .Set("list", extensions::ListBuilder()
                                    .Append(extensions::DictionaryBuilder()
                                                .Set("one", 1)
                                                .Set("two", 2)
                                                .Build())
                                    .Append(extensions::DictionaryBuilder()
                                                .Set("three", 3)
                                                .Build())
                                    .Build())
                   .Build())
          .Build();
  SetPolicies(*policy);
  // Now run the extension.
  ASSERT_TRUE(RunExtensionTest("settings/managed_storage")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, PRE_ManagedStorageEvents) {
  ResultCatcher catcher;

  // This test starts without any test extensions installed.
  EXPECT_FALSE(GetSingleLoadedExtension());
  message_.clear();

  // Set policies for the test extension.
  std::unique_ptr<base::DictionaryValue> policy =
      extensions::DictionaryBuilder()
          .Set("constant-policy", "aaa")
          .Set("changes-policy", "bbb")
          .Set("deleted-policy", "ccc")
          .Build();
  SetPolicies(*policy);

  ExtensionTestMessageListener ready_listener("ready", false);
  // Load the extension to install the event listener.
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("settings/managed_storage_events"));
  ASSERT_TRUE(extension);
  // Wait until the extension sends the "ready" message.
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // Now change the policies and wait until the extension is done.
  policy = extensions::DictionaryBuilder()
      .Set("constant-policy", "aaa")
      .Set("changes-policy", "ddd")
      .Set("new-policy", "eee")
      .Build();
  SetPolicies(*policy);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, ManagedStorageEvents) {
  // This test runs after PRE_ManagedStorageEvents without having deleted the
  // profile, so the extension is still around. While the browser restarted the
  // policy went back to the empty default, and so the extension should receive
  // the corresponding change events.

  ResultCatcher catcher;

  // Verify that the test extension is still installed.
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);
  EXPECT_EQ(kManagedStorageExtensionId, extension->id());

  // Running the test again skips the onInstalled callback, and just triggers
  // the onChanged notification.
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, ManagedStorageDisabled) {
  // Disable the 'managed' namespace.
  StorageFrontend* frontend = StorageFrontend::Get(browser()->profile());
  frontend->DisableStorageForTesting(settings_namespace::MANAGED);
  EXPECT_FALSE(frontend->IsStorageEnabled(settings_namespace::MANAGED));
  // Now run the extension.
  ASSERT_TRUE(RunExtensionTest("settings/managed_storage_disabled"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, StorageAreaOnChanged) {
  ASSERT_TRUE(RunExtensionTest("settings/storage_area")) << message_;
}

}  // namespace extensions
