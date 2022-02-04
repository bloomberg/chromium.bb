// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/ntp_custom_background_service.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_background_service_factory.h"
#include "chrome/browser/search/background/ntp_custom_background_service_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/url_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace {

const char kNtpCustomBackgroundURL[] = "background_url";
const char kNtpCustomBackgroundAttributionLine1[] = "attribution_line_1";
const char kNtpCustomBackgroundAttributionLine2[] = "attribution_line_2";
const char kNtpCustomBackgroundAttributionActionURL[] =
    "attribution_action_url";
const char kNtpCustomBackgroundCollectionId[] = "collection_id";
const char kNtpCustomBackgroundResumeToken[] = "resume_token";
const char kNtpCustomBackgroundRefreshTimestamp[] = "refresh_timestamp";

base::DictionaryValue GetBackgroundInfoAsDict(
    const GURL& background_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& action_url,
    const absl::optional<std::string>& collection_id,
    const absl::optional<std::string>& resume_token,
    const absl::optional<int> refresh_timestamp) {
  base::DictionaryValue background_info;
  background_info.SetKey(kNtpCustomBackgroundURL,
                         base::Value(background_url.spec()));
  background_info.SetKey(kNtpCustomBackgroundAttributionLine1,
                         base::Value(attribution_line_1));
  background_info.SetKey(kNtpCustomBackgroundAttributionLine2,
                         base::Value(attribution_line_2));
  background_info.SetKey(kNtpCustomBackgroundAttributionActionURL,
                         base::Value(action_url.spec()));
  background_info.SetKey(kNtpCustomBackgroundCollectionId,
                         base::Value(collection_id.value_or("")));
  background_info.SetKey(kNtpCustomBackgroundResumeToken,
                         base::Value(resume_token.value_or("")));
  background_info.SetKey(kNtpCustomBackgroundRefreshTimestamp,
                         base::Value(refresh_timestamp.value_or(0)));

  return background_info;
}

base::Value NtpCustomBackgroundDefaults() {
  base::Value defaults(base::Value::Type::DICTIONARY);
  defaults.SetKey(kNtpCustomBackgroundURL,
                  base::Value(base::Value::Type::STRING));
  defaults.SetKey(kNtpCustomBackgroundAttributionLine1,
                  base::Value(base::Value::Type::STRING));
  defaults.SetKey(kNtpCustomBackgroundAttributionLine2,
                  base::Value(base::Value::Type::STRING));
  defaults.SetKey(kNtpCustomBackgroundAttributionActionURL,
                  base::Value(base::Value::Type::STRING));
  defaults.SetKey(kNtpCustomBackgroundCollectionId,
                  base::Value(base::Value::Type::STRING));
  defaults.SetKey(kNtpCustomBackgroundResumeToken,
                  base::Value(base::Value::Type::STRING));
  defaults.SetKey(kNtpCustomBackgroundRefreshTimestamp,
                  base::Value(base::Value::Type::INTEGER));
  return defaults;
}

void CopyFileToProfilePath(const base::FilePath& from_path,
                           const base::FilePath& profile_path) {
  base::CopyFile(from_path,
                 profile_path.AppendASCII(
                     chrome::kChromeUIUntrustedNewTabPageBackgroundFilename));
}

void RemoveLocalBackgroundImageCopy(Profile* profile) {
  base::FilePath path = profile->GetPath().AppendASCII(
      chrome::kChromeUIUntrustedNewTabPageBackgroundFilename);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(base::GetDeleteFileCallback(), path));
}

}  // namespace

// static
void NtpCustomBackgroundService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(
      prefs::kNtpCustomBackgroundDict, NtpCustomBackgroundDefaults(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kNtpCustomBackgroundLocalToDevice,
                                false);
}

// static
void NtpCustomBackgroundService::ResetProfilePrefs(Profile* profile) {
  profile->GetPrefs()->ClearPref(prefs::kNtpCustomBackgroundDict);
  profile->GetPrefs()->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice,
                                  false);
  RemoveLocalBackgroundImageCopy(profile);
}

NtpCustomBackgroundService::NtpCustomBackgroundService(Profile* profile)
    : profile_(profile),
      pref_service_(profile_->GetPrefs()),
      clock_(base::DefaultClock::GetInstance()),
      background_updated_timestamp_(base::TimeTicks::Now()) {
  background_service_ = NtpBackgroundServiceFactory::GetForProfile(profile_);
  if (background_service_)
    background_service_observation_.Observe(background_service_.get());

  // Update theme info when the pref is changed via Sync.
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kNtpCustomBackgroundDict,
      base::BindRepeating(&NtpCustomBackgroundService::UpdateBackgroundFromSync,
                          weak_ptr_factory_.GetWeakPtr()));
}

NtpCustomBackgroundService::~NtpCustomBackgroundService() = default;

void NtpCustomBackgroundService::Shutdown() {
  for (NtpCustomBackgroundServiceObserver& observer : observers_)
    observer.OnNtpCustomBackgroundServiceShuttingDown();
}

void NtpCustomBackgroundService::OnCollectionInfoAvailable() {}

void NtpCustomBackgroundService::OnCollectionImagesAvailable() {}

void NtpCustomBackgroundService::OnNextCollectionImageAvailable() {
  auto image = background_service_->next_image();
  std::string attribution1;
  std::string attribution2;
  if (image.attribution.size() > 0)
    attribution1 = image.attribution[0];
  if (image.attribution.size() > 1)
    attribution2 = image.attribution[1];

  std::string resume_token = background_service_->next_image_resume_token();
  int64_t timestamp = (clock_->Now() + base::Days(1)).ToTimeT();

  base::DictionaryValue background_info = GetBackgroundInfoAsDict(
      image.image_url, attribution1, attribution2, image.attribution_action_url,
      image.collection_id, resume_token, timestamp);

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  pref_service_->Set(prefs::kNtpCustomBackgroundDict, background_info);
}

void NtpCustomBackgroundService::OnNtpBackgroundServiceShuttingDown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  background_service_observation_.Reset();
  background_service_ = nullptr;
}

void NtpCustomBackgroundService::UpdateBackgroundFromSync() {
  // Any incoming change to synced background data should clear the local image.
  pref_service_->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice, false);
  RemoveLocalBackgroundImageCopy(profile_);
  NotifyAboutBackgrounds();
}

void NtpCustomBackgroundService::ResetCustomBackgroundInfo() {
  SetCustomBackgroundInfo(GURL(), std::string(), std::string(), GURL(),
                          std::string());
}

void NtpCustomBackgroundService::SetCustomBackgroundInfo(
    const GURL& background_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& action_url,
    const std::string& collection_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (IsCustomBackgroundDisabledByPolicy()) {
    return;
  }
  // Store current background info before it is changed so it can be used if
  // RevertBackgroundChanges is called.
  if (previous_background_info_ == absl::nullopt) {
    previous_background_info_ = absl::make_optional(
        pref_service_->Get(prefs::kNtpCustomBackgroundDict)->Clone());
    previous_local_background_ = false;
  }

  bool is_backdrop_collection =
      background_service_ &&
      background_service_->IsValidBackdropCollection(collection_id);
  bool is_backdrop_url =
      background_service_ &&
      background_service_->IsValidBackdropUrl(background_url);

  bool need_forced_refresh =
      pref_service_->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice) &&
      pref_service_->FindPreference(prefs::kNtpCustomBackgroundDict)
          ->IsDefaultValue();
  pref_service_->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice, false);
  RemoveLocalBackgroundImageCopy(profile_);

  background_updated_timestamp_ = base::TimeTicks::Now();

  if (!collection_id.empty() && is_backdrop_collection) {
    background_service_->FetchNextCollectionImage(collection_id, absl::nullopt);
  } else if (background_url.is_valid() && is_backdrop_url) {
    base::DictionaryValue background_info = GetBackgroundInfoAsDict(
        background_url, attribution_line_1, attribution_line_2, action_url,
        absl::nullopt, absl::nullopt, absl::nullopt);
    pref_service_->Set(prefs::kNtpCustomBackgroundDict, background_info);
  } else {
    pref_service_->ClearPref(prefs::kNtpCustomBackgroundDict);

    // If this device was using a local image and did not have a non-local
    // background saved, UpdateBackgroundFromSync will not fire. Therefore, we
    // need to force a refresh here.
    if (need_forced_refresh) {
      NotifyAboutBackgrounds();
    }
  }
}

void NtpCustomBackgroundService::SelectLocalBackgroundImage(
    const base::FilePath& path) {
  if (IsCustomBackgroundDisabledByPolicy()) {
    return;
  }
  previous_background_info_.reset();
  previous_local_background_ = true;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&CopyFileToProfilePath, path, profile_->GetPath()),
      base::BindOnce(&NtpCustomBackgroundService::SetBackgroundToLocalResource,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NtpCustomBackgroundService::RefreshBackgroundIfNeeded() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value* background_info =
      profile_->GetPrefs()->GetDictionary(prefs::kNtpCustomBackgroundDict);
  int64_t refresh_timestamp = 0;
  const base::Value* timestamp_value =
      background_info->FindKey(kNtpCustomBackgroundRefreshTimestamp);
  if (timestamp_value)
    refresh_timestamp = timestamp_value->GetInt();
  if (refresh_timestamp == 0)
    return;

  if (clock_->Now().ToTimeT() > refresh_timestamp) {
    std::string collection_id =
        background_info->FindKey(kNtpCustomBackgroundCollectionId)->GetString();
    std::string resume_token =
        background_info->FindKey(kNtpCustomBackgroundResumeToken)->GetString();
    background_service_->FetchNextCollectionImage(collection_id, resume_token);
  }
}

absl::optional<CustomBackground>
NtpCustomBackgroundService::GetCustomBackground() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (pref_service_->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice)) {
    auto custom_background = absl::make_optional<CustomBackground>();
    // Add a timestamp to the url to prevent the browser from using a cached
    // version when "Upload an image" is used multiple times.
    std::string time_string = std::to_string(base::Time::Now().ToTimeT());
    std::string local_string(chrome::kChromeUIUntrustedNewTabPageBackgroundUrl);
    GURL timestamped_url(local_string + "?ts=" + time_string);
    custom_background->custom_background_url = timestamped_url;
    custom_background->custom_background_attribution_line_1 = std::string();
    custom_background->custom_background_attribution_line_2 = std::string();
    custom_background->custom_background_attribution_action_url = GURL();
    return custom_background;
  }

  // Attempt to get custom background URL from preferences.
  if (IsCustomBackgroundPrefValid()) {
    auto custom_background = absl::make_optional<CustomBackground>();
    const base::Value* background_info =
        pref_service_->GetDictionary(prefs::kNtpCustomBackgroundDict);
    GURL custom_background_url(
        background_info->FindKey(kNtpCustomBackgroundURL)->GetString());

    std::string collection_id;
    const base::Value* id_value =
        background_info->FindKey(kNtpCustomBackgroundCollectionId);
    if (id_value)
      collection_id = id_value->GetString();

    // Set custom background information in theme info (attributions are
    // optional).
    const base::Value* attribution_line_1 =
        background_info->FindKey(kNtpCustomBackgroundAttributionLine1);
    const base::Value* attribution_line_2 =
        background_info->FindKey(kNtpCustomBackgroundAttributionLine2);
    const base::Value* attribution_action_url =
        background_info->FindKey(kNtpCustomBackgroundAttributionActionURL);
    custom_background->custom_background_url = custom_background_url;
    custom_background->collection_id = collection_id;

    if (attribution_line_1) {
      custom_background->custom_background_attribution_line_1 =
          background_info->FindKey(kNtpCustomBackgroundAttributionLine1)
              ->GetString();
    }
    if (attribution_line_2) {
      custom_background->custom_background_attribution_line_2 =
          background_info->FindKey(kNtpCustomBackgroundAttributionLine2)
              ->GetString();
    }
    if (attribution_action_url) {
      GURL action_url(
          background_info->FindKey(kNtpCustomBackgroundAttributionActionURL)
              ->GetString());

      if (!action_url.SchemeIsCryptographic()) {
        custom_background->custom_background_attribution_action_url = GURL();
      } else {
        custom_background->custom_background_attribution_action_url =
            action_url;
      }
    }
    return custom_background;
  }

  return absl::nullopt;
}

void NtpCustomBackgroundService::AddObserver(
    NtpCustomBackgroundServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void NtpCustomBackgroundService::RemoveObserver(
    NtpCustomBackgroundServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool NtpCustomBackgroundService::IsCustomBackgroundDisabledByPolicy() {
  // |prefs::kNtpCustomBackgroundDict| is managed by policy only if
  // |policy::key::kNTPCustomBackgroundEnabled| is set to false and therefore
  // should be empty.
  bool managed =
      pref_service_->IsManagedPreference(prefs::kNtpCustomBackgroundDict);
  if (managed) {
    DCHECK(pref_service_->GetDictionary(prefs::kNtpCustomBackgroundDict)
               ->DictEmpty());
  }
  return managed;
}

bool NtpCustomBackgroundService::IsCustomBackgroundSet() {
  return pref_service_->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice) ||
         IsCustomBackgroundPrefValid();
}

void NtpCustomBackgroundService::AddValidBackdropUrlForTesting(
    const GURL& url) const {
  background_service_->AddValidBackdropUrlForTesting(url);
}

void NtpCustomBackgroundService::AddValidBackdropCollectionForTesting(
    const std::string& collection_id) const {
  background_service_->AddValidBackdropCollectionForTesting(collection_id);
}

void NtpCustomBackgroundService::SetNextCollectionImageForTesting(
    const CollectionImage& image) const {
  background_service_->SetNextCollectionImageForTesting(image);
}

void NtpCustomBackgroundService::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

void NtpCustomBackgroundService::SetBackgroundToLocalResource() {
  background_updated_timestamp_ = base::TimeTicks::Now();
  pref_service_->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice, true);
  NotifyAboutBackgrounds();
}

bool NtpCustomBackgroundService::IsCustomBackgroundPrefValid() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value* background_info =
      profile_->GetPrefs()->GetDictionary(prefs::kNtpCustomBackgroundDict);
  if (!background_info)
    return false;

  const base::Value* background_url =
      background_info->FindKey(kNtpCustomBackgroundURL);
  if (!background_url)
    return false;

  return GURL(background_url->GetString()).is_valid();
}

void NtpCustomBackgroundService::NotifyAboutBackgrounds() {
  for (NtpCustomBackgroundServiceObserver& observer : observers_)
    observer.OnCustomBackgroundImageUpdated();
}

void NtpCustomBackgroundService::RevertBackgroundChanges() {
  if (previous_background_info_.has_value()) {
    pref_service_->Set(prefs::kNtpCustomBackgroundDict,
                       *previous_background_info_);
  }
  if (previous_local_background_) {
    SetBackgroundToLocalResource();
  }
  previous_background_info_.reset();
  previous_local_background_ = false;
}

void NtpCustomBackgroundService::ConfirmBackgroundChanges() {
  previous_background_info_.reset();
  previous_local_background_ = false;
}
