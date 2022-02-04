// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/registry.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/ash/file_system_provider/icon_set.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace file_system_provider {
namespace {

const char kTemporaryOrigin[] =
    "chrome-extension://abcabcabcabcabcabcabcabcabcabcabcabca/";
const char kPersistentOrigin[] =
    "chrome-extension://efgefgefgefgefgefgefgefgefgefgefgefge/";
const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kDisplayName[] = "Camera Pictures";
const ProviderId kProviderId = ProviderId::CreateFromExtensionId(kExtensionId);

// The dot in the file system ID is there in order to check that saving to
// preferences works correctly. File System ID is used as a key in
// a base::Value, so it has to be stored without path expansion.
const char kFileSystemId[] = "camera/pictures/id .!@#$%^&*()_+";

const int kOpenedFilesLimit = 5;

// Stores a provided file system information in preferences together with a
// fake watcher.
void RememberFakeFileSystem(TestingProfile* profile,
                            const ProviderId& provider_id,
                            const std::string& file_system_id,
                            const std::string& display_name,
                            bool writable,
                            bool supports_notify_tag,
                            int opened_files_limit,
                            const Watcher& watcher) {
  // Warning. Updating this code means that backward compatibility may be
  // broken, what is unexpected and should be avoided.
  sync_preferences::TestingPrefServiceSyncable* const pref_service =
      profile->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  base::Value extensions{base::Value::Type::DICTIONARY};
  base::Value file_system{base::Value::Type::DICTIONARY};
  file_system.SetKey(kPrefKeyFileSystemId, base::Value(kFileSystemId));
  file_system.SetKey(kPrefKeyDisplayName, base::Value(kDisplayName));
  file_system.SetKey(kPrefKeyWritable, base::Value(writable));
  file_system.SetKey(kPrefKeySupportsNotifyTag,
                     base::Value(supports_notify_tag));
  file_system.SetKey(kPrefKeyOpenedFilesLimit, base::Value(opened_files_limit));

  // Remember watchers.
  base::Value watcher_value{base::Value::Type::DICTIONARY};
  watcher_value.SetKey(kPrefKeyWatcherEntryPath,
                       base::Value(watcher.entry_path.value()));
  watcher_value.SetKey(kPrefKeyWatcherRecursive,
                       base::Value(watcher.recursive));
  watcher_value.SetKey(kPrefKeyWatcherLastTag, base::Value(watcher.last_tag));
  base::Value persistent_origins_value{base::Value::Type::LIST};
  for (const auto& subscriber_it : watcher.subscribers) {
    if (subscriber_it.second.persistent)
      persistent_origins_value.Append(subscriber_it.first.spec());
  }

  watcher_value.SetKey(kPrefKeyWatcherPersistentOrigins,
                       std::move(persistent_origins_value));
  base::Value watchers{base::Value::Type::DICTIONARY};
  watchers.SetKey(watcher.entry_path.value(), std::move(watcher_value));
  file_system.SetKey(kPrefKeyWatchers, std::move(watchers));
  base::Value file_systems{base::Value::Type::DICTIONARY};
  file_systems.SetKey(kFileSystemId, std::move(file_system));
  extensions.SetKey(kProviderId.ToString(), std::move(file_systems));
  pref_service->Set(prefs::kFileSystemProviderMounted, std::move(extensions));
}

}  // namespace

class FileSystemProviderRegistryTest : public testing::Test {
 protected:
  FileSystemProviderRegistryTest() : profile_(NULL) {}

  ~FileSystemProviderRegistryTest() override {}

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test-user@example.com");
    registry_ = std::make_unique<Registry>(profile_);
    fake_watcher_.entry_path = base::FilePath(FILE_PATH_LITERAL("/a/b/c"));
    fake_watcher_.recursive = true;
    fake_watcher_.subscribers[GURL(kTemporaryOrigin)].origin =
        GURL(kTemporaryOrigin);
    fake_watcher_.subscribers[GURL(kTemporaryOrigin)].persistent = false;
    fake_watcher_.subscribers[GURL(kPersistentOrigin)].origin =
        GURL(kPersistentOrigin);
    fake_watcher_.subscribers[GURL(kPersistentOrigin)].persistent = true;
    fake_watcher_.last_tag = "hello-world";
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
  std::unique_ptr<RegistryInterface> registry_;
  Watcher fake_watcher_;
};

TEST_F(FileSystemProviderRegistryTest, RestoreFileSystems) {
  // Create a fake entry in the preferences.
  RememberFakeFileSystem(profile_, kProviderId, kFileSystemId, kDisplayName,
                         true /* writable */, true /* supports_notify_tag */,
                         kOpenedFilesLimit, fake_watcher_);

  std::unique_ptr<RegistryInterface::RestoredFileSystems>
      restored_file_systems = registry_->RestoreFileSystems(kProviderId);

  ASSERT_EQ(1u, restored_file_systems->size());
  const RegistryInterface::RestoredFileSystem& restored_file_system =
      restored_file_systems->at(0);
  EXPECT_EQ(kProviderId, restored_file_system.provider_id);
  EXPECT_EQ(kFileSystemId, restored_file_system.options.file_system_id);
  EXPECT_EQ(kDisplayName, restored_file_system.options.display_name);
  EXPECT_TRUE(restored_file_system.options.writable);
  EXPECT_TRUE(restored_file_system.options.supports_notify_tag);
  EXPECT_EQ(kOpenedFilesLimit, restored_file_system.options.opened_files_limit);

  ASSERT_EQ(1u, restored_file_system.watchers.size());
  const auto& restored_watcher_it = restored_file_system.watchers.find(
      WatcherKey(fake_watcher_.entry_path, fake_watcher_.recursive));
  ASSERT_NE(restored_file_system.watchers.end(), restored_watcher_it);

  EXPECT_EQ(fake_watcher_.entry_path, restored_watcher_it->second.entry_path);
  EXPECT_EQ(fake_watcher_.recursive, restored_watcher_it->second.recursive);
  EXPECT_EQ(fake_watcher_.last_tag, restored_watcher_it->second.last_tag);
}

TEST_F(FileSystemProviderRegistryTest, RememberFileSystem) {
  MountOptions options(kFileSystemId, kDisplayName);
  options.writable = true;
  options.supports_notify_tag = true;
  options.opened_files_limit = kOpenedFilesLimit;

  ProvidedFileSystemInfo file_system_info(
      kProviderId, options, base::FilePath(FILE_PATH_LITERAL("/a/b/c")),
      false /* configurable */, true /* watchable */, extensions::SOURCE_FILE,
      IconSet());

  Watchers watchers;
  watchers[WatcherKey(fake_watcher_.entry_path, fake_watcher_.recursive)] =
      fake_watcher_;

  registry_->RememberFileSystem(file_system_info, watchers);

  sync_preferences::TestingPrefServiceSyncable* const pref_service =
      profile_->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  const base::Value* const extensions =
      pref_service->GetDictionary(prefs::kFileSystemProviderMounted);
  ASSERT_TRUE(extensions);

  const base::Value* file_systems =
      extensions->FindDictKey(kProviderId.ToString());
  ASSERT_TRUE(file_systems);
  EXPECT_EQ(1u, file_systems->DictSize());

  const base::Value* file_system = file_systems->FindKey(kFileSystemId);
  ASSERT_TRUE(file_system);

  const std::string* file_system_id =
      file_system->FindStringKey(kPrefKeyFileSystemId);
  EXPECT_TRUE(file_system_id);
  EXPECT_EQ(kFileSystemId, *file_system_id);

  const std::string* display_name =
      file_system->FindStringKey(kPrefKeyDisplayName);
  EXPECT_TRUE(display_name);
  EXPECT_EQ(kDisplayName, *display_name);

  absl::optional<bool> writable = file_system->FindBoolKey(kPrefKeyWritable);
  EXPECT_TRUE(writable.has_value());
  EXPECT_TRUE(writable.value());

  absl::optional<bool> supports_notify_tag =
      file_system->FindBoolKey(kPrefKeySupportsNotifyTag);
  EXPECT_TRUE(supports_notify_tag.has_value());
  EXPECT_TRUE(supports_notify_tag.value());

  absl::optional<int> opened_files_limit =
      file_system->FindIntKey(kPrefKeyOpenedFilesLimit);
  EXPECT_TRUE(opened_files_limit.has_value());
  EXPECT_EQ(kOpenedFilesLimit, opened_files_limit.value());

  const base::Value* watchers_value =
      file_system->FindDictKey(kPrefKeyWatchers);
  ASSERT_TRUE(watchers_value);

  const base::Value* watcher =
      watchers_value->FindDictKey(fake_watcher_.entry_path.value());
  ASSERT_TRUE(watcher);

  const std::string* entry_path =
      watcher->FindStringKey(kPrefKeyWatcherEntryPath);
  EXPECT_TRUE(entry_path);
  EXPECT_EQ(fake_watcher_.entry_path.value(), *entry_path);

  absl::optional<bool> recursive =
      watcher->FindBoolKey(kPrefKeyWatcherRecursive);
  EXPECT_TRUE(recursive.has_value());
  EXPECT_EQ(fake_watcher_.recursive, recursive.value());

  const std::string* last_tag = watcher->FindStringKey(kPrefKeyWatcherLastTag);
  EXPECT_TRUE(last_tag);
  EXPECT_EQ(fake_watcher_.last_tag, *last_tag);

  const base::Value* persistent_origins =
      watcher->FindListKey(kPrefKeyWatcherPersistentOrigins);
  ASSERT_TRUE(persistent_origins);
  ASSERT_GT(fake_watcher_.subscribers.size(),
            persistent_origins->GetList().size());
  ASSERT_EQ(1u, persistent_origins->GetList().size());
  const std::string* persistent_origin =
      persistent_origins->GetList()[0].GetIfString();
  ASSERT_TRUE(persistent_origin);
  const auto& fake_subscriber_it =
      fake_watcher_.subscribers.find(GURL(*persistent_origin));
  ASSERT_NE(fake_watcher_.subscribers.end(), fake_subscriber_it);
  EXPECT_TRUE(fake_subscriber_it->second.persistent);
}

TEST_F(FileSystemProviderRegistryTest, ForgetFileSystem) {
  // Create a fake file systems in the preferences.
  RememberFakeFileSystem(profile_, kProviderId, kFileSystemId, kDisplayName,
                         true /* writable */, true /* supports_notify_tag */,
                         kOpenedFilesLimit, fake_watcher_);

  registry_->ForgetFileSystem(kProviderId, kFileSystemId);

  sync_preferences::TestingPrefServiceSyncable* const pref_service =
      profile_->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  const base::Value* const extensions =
      pref_service->GetDictionary(prefs::kFileSystemProviderMounted);
  ASSERT_TRUE(extensions);

  const base::Value* file_systems =
      extensions->FindDictKey(kProviderId.GetExtensionId());
  EXPECT_FALSE(file_systems);
}

TEST_F(FileSystemProviderRegistryTest, UpdateWatcherTag) {
  MountOptions options(kFileSystemId, kDisplayName);
  options.writable = true;
  options.supports_notify_tag = true;

  ProvidedFileSystemInfo file_system_info(
      kProviderId, options, base::FilePath(FILE_PATH_LITERAL("/a/b/c")),
      false /* configurable */, true /* watchable */, extensions::SOURCE_FILE,
      IconSet());

  Watchers watchers;
  watchers[WatcherKey(fake_watcher_.entry_path, fake_watcher_.recursive)] =
      fake_watcher_;

  registry_->RememberFileSystem(file_system_info, watchers);

  fake_watcher_.last_tag = "updated-tag";
  registry_->UpdateWatcherTag(file_system_info, fake_watcher_);

  sync_preferences::TestingPrefServiceSyncable* const pref_service =
      profile_->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  const base::Value* const extensions =
      pref_service->GetDictionary(prefs::kFileSystemProviderMounted);
  ASSERT_TRUE(extensions);

  const base::Value* file_systems =
      extensions->FindDictKey(kProviderId.ToString());
  ASSERT_TRUE(file_systems);
  EXPECT_EQ(1u, file_systems->DictSize());

  const base::Value* file_system = file_systems->FindKey(kFileSystemId);
  ASSERT_TRUE(file_system);

  const base::Value* watchers_value =
      file_system->FindDictKey(kPrefKeyWatchers);
  ASSERT_TRUE(watchers_value);

  const base::Value* watcher =
      watchers_value->FindDictKey(fake_watcher_.entry_path.value());
  ASSERT_TRUE(watcher);

  const std::string* last_tag = watcher->FindStringKey(kPrefKeyWatcherLastTag);
  EXPECT_TRUE(last_tag);
  EXPECT_EQ(fake_watcher_.last_tag, *last_tag);
}

}  // namespace file_system_provider
}  // namespace ash
