// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_profile_manager.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_file_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/account_manager/account_profile_mapper.h"
#endif

const char kGuestProfileName[] = "Guest";
const char kSystemProfileName[] = "System";

TestingProfileManager::TestingProfileManager(TestingBrowserProcess* process)
    : called_set_up_(false),
      browser_process_(process),
      owned_local_state_(std::make_unique<ScopedTestingLocalState>(process)),
      profile_manager_(nullptr) {
  local_state_ = owned_local_state_.get();
}

TestingProfileManager::TestingProfileManager(
    TestingBrowserProcess* process,
    ScopedTestingLocalState* local_state)
    : called_set_up_(false),
      browser_process_(process),
      local_state_(local_state),
      profile_manager_(nullptr) {}

TestingProfileManager::~TestingProfileManager() {
  // Destroying this class also destroys the LocalState, so make sure the
  // associated ProfileManager is also destroyed.
  browser_process_->SetProfileManager(nullptr);
}

bool TestingProfileManager::SetUp(const base::FilePath& profiles_path) {
  SetUpInternal(profiles_path);
  return called_set_up_;
}

TestingProfile* TestingProfileManager::CreateTestingProfile(
    const std::string& profile_name,
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs,
    const std::u16string& user_name,
    int avatar_id,
    const std::string& supervised_user_id,
    TestingProfile::TestingFactories testing_factories,
    absl::optional<bool> is_new_profile,
    absl::optional<std::unique_ptr<policy::PolicyService>> policy_service) {
  DCHECK(called_set_up_);

  // Create a path for the profile based on the name.
  base::FilePath profile_path(profiles_path_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (profile_name != chrome::kInitialProfile &&
      profile_name != chrome::kLockScreenProfile &&
      profile_name != chromeos::ProfileHelper::GetLockScreenAppProfileName()) {
    profile_path =
        profile_path.Append(chromeos::ProfileHelper::Get()->GetUserProfileDir(
            chromeos::ProfileHelper::GetUserIdHashByUserIdForTesting(
                profile_name)));
  } else {
    profile_path = profile_path.AppendASCII(profile_name);
  }
#else
  profile_path = profile_path.AppendASCII(profile_name);
#endif

  // Create the profile and register it.
  TestingProfile::Builder builder;
  builder.SetPath(profile_path);
  builder.SetPrefService(std::move(prefs));
  builder.SetSupervisedUserId(supervised_user_id);
  builder.SetProfileName(profile_name);
  builder.SetIsNewProfile(is_new_profile.value_or(false));
  if (policy_service)
    builder.SetPolicyService(std::move(*policy_service));

  for (TestingProfile::TestingFactories::value_type& pair : testing_factories)
    builder.AddTestingFactory(pair.first, std::move(pair.second));
  testing_factories.clear();

  std::unique_ptr<TestingProfile> profile = builder.Build();
  TestingProfile* profile_ptr = profile.get();
  profile_manager_->AddProfile(std::move(profile));

  // Update the user metadata.
  ProfileAttributesEntry* entry =
      profile_manager_->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_path);
  DCHECK(entry);
  entry->SetAvatarIconIndex(avatar_id);
  entry->SetSupervisedUserId(supervised_user_id);
  entry->SetLocalProfileName(user_name, entry->IsUsingDefaultName());

  testing_profiles_.insert(std::make_pair(profile_name, profile_ptr));
  profile_observations_.AddObservation(profile_ptr);

  return profile_ptr;
}

TestingProfile* TestingProfileManager::CreateTestingProfile(
    const std::string& name) {
  DCHECK(called_set_up_);
  return CreateTestingProfile(name, /*testing_factories=*/{});
}

TestingProfile* TestingProfileManager::CreateTestingProfile(
    const std::string& name,
    TestingProfile::TestingFactories testing_factories) {
  DCHECK(called_set_up_);
  return CreateTestingProfile(
      name, std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
      base::UTF8ToUTF16(name), 0, std::string(), std::move(testing_factories));
}

TestingProfile* TestingProfileManager::CreateGuestProfile() {
  DCHECK(called_set_up_);

  // Create the profile and register it.
  TestingProfile::Builder builder;
  builder.SetGuestSession();
  builder.SetPath(ProfileManager::GetGuestProfilePath());

  // Add the guest profile to the profile manager, but not to the attributes
  // storage.
  std::unique_ptr<TestingProfile> profile = builder.Build();
  TestingProfile* profile_ptr = profile.get();
  profile_ptr->set_profile_name(kGuestProfileName);

  // Set up a profile with an off the record profile.
  TestingProfile::Builder off_the_record_builder;
  off_the_record_builder.SetGuestSession();
  off_the_record_builder.BuildIncognito(profile_ptr);

  profile_manager_->AddProfile(std::move(profile));
  profile_manager_->SetNonPersonalProfilePrefs(profile_ptr);

  testing_profiles_.insert(std::make_pair(kGuestProfileName, profile_ptr));
  profile_observations_.AddObservation(profile_ptr);

  return profile_ptr;
}

TestingProfile* TestingProfileManager::CreateSystemProfile() {
  DCHECK(called_set_up_);

  // Create the profile and register it.
  TestingProfile::Builder builder;
  builder.SetPath(ProfileManager::GetSystemProfilePath());

  // Add the system profile to the profile manager, but not to the attributes
  // storage.
  std::unique_ptr<TestingProfile> profile = builder.Build();
  TestingProfile* profile_ptr = profile.get();
  profile_ptr->set_profile_name(kSystemProfileName);

  profile_manager_->AddProfile(std::move(profile));

  testing_profiles_.insert(std::make_pair(kSystemProfileName, profile_ptr));

  return profile_ptr;
}

void TestingProfileManager::DeleteTestingProfile(const std::string& name) {
  DCHECK(called_set_up_);

  auto it = testing_profiles_.find(name);
  if (it == testing_profiles_.end()) {
    // Profile was already deleted, probably due to the
    // DestroyProfileOnBrowserClose flag.
    DCHECK(
        base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose));
    return;
  }

  TestingProfile* profile = it->second;

  profile_manager_->GetProfileAttributesStorage().RemoveProfile(
      profile->GetPath());
  profile_manager_->profiles_info_.erase(profile->GetPath());
}

void TestingProfileManager::DeleteAllTestingProfiles() {
  DCHECK(called_set_up_);

  ProfileAttributesStorage& storage =
      profile_manager_->GetProfileAttributesStorage();
  for (auto& name_profile_pair : testing_profiles_) {
    TestingProfile* profile = name_profile_pair.second;
    if (profile->IsGuestSession() || profile->IsSystemProfile()) {
      // Guest and System profiles aren't added to Storage.
      continue;
    }
    storage.RemoveProfile(profile->GetPath());
  }
  profile_manager_->profiles_info_.clear();
}


void TestingProfileManager::DeleteGuestProfile() {
  DCHECK(called_set_up_);

  auto it = testing_profiles_.find(kGuestProfileName);
  DCHECK(it != testing_profiles_.end());

  profile_manager_->profiles_info_.erase(ProfileManager::GetGuestProfilePath());
}

void TestingProfileManager::DeleteSystemProfile() {
  DCHECK(called_set_up_);

  auto it = testing_profiles_.find(kSystemProfileName);
  DCHECK(it != testing_profiles_.end());

  profile_manager_->profiles_info_.erase(
      ProfileManager::GetSystemProfilePath());
}

void TestingProfileManager::DeleteProfileAttributesStorage() {
  profile_manager_->profile_attributes_storage_.reset(nullptr);
}

void TestingProfileManager::UpdateLastUser(Profile* last_active) {
#if !defined(OS_ANDROID)
  profile_manager_->UpdateLastUser(last_active);
#endif
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void TestingProfileManager::SetAccountProfileMapper(
    std::unique_ptr<AccountProfileMapper> mapper) {
  DCHECK(!profile_manager_->account_profile_mapper_)
      << "AccountProfileMapper must be set before the first usage";
  profile_manager_->account_profile_mapper_ = std::move(mapper);
}
#endif

const base::FilePath& TestingProfileManager::profiles_dir() {
  DCHECK(called_set_up_);
  return profiles_path_;
}

ProfileManager* TestingProfileManager::profile_manager() {
  DCHECK(called_set_up_);
  return profile_manager_;
}

ProfileAttributesStorage* TestingProfileManager::profile_attributes_storage() {
  DCHECK(called_set_up_);
  return &profile_manager_->GetProfileAttributesStorage();
}

void TestingProfileManager::OnProfileWillBeDestroyed(Profile* profile) {
  testing_profiles_.erase(profile->GetProfileUserName());
  profile_observations_.RemoveObservation(profile);
}

void TestingProfileManager::SetUpInternal(const base::FilePath& profiles_path) {
  ASSERT_FALSE(browser_process_->profile_manager())
      << "ProfileManager already exists";

  // Set up the directory for profiles.
  if (profiles_path.empty()) {
    profiles_path_ = base::CreateUniqueTempDirectoryScopedToTest();
  } else {
    profiles_path_ = profiles_path;
  }
  user_data_dir_override_ = std::make_unique<base::ScopedPathOverride>(
      chrome::DIR_USER_DATA, profiles_path_);

  auto profile_manager_unique =
      std::make_unique<FakeProfileManager>(profiles_path_);
  profile_manager_ = profile_manager_unique.get();
  browser_process_->SetProfileManager(std::move(profile_manager_unique));

  profile_manager_->GetProfileAttributesStorage()
      .set_disable_avatar_download_for_testing(true);
  called_set_up_ = true;
}
