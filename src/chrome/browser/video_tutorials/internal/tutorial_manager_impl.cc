// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/tutorial_manager_impl.h"

#include <set>

#include "base/bind.h"
#include "base/stl_util.h"
#include "chrome/browser/video_tutorials/internal/config.h"
#include "chrome/browser/video_tutorials/prefs.h"
#include "components/prefs/pref_service.h"

namespace video_tutorials {
namespace {

std::vector<Tutorial> FilterTutorials(const std::vector<Tutorial>& tutorials) {
  std::vector<Tutorial> tutorials_excluding_summary;
  for (const auto& tutorial : tutorials) {
    if (tutorial.feature == FeatureType::kSummary)
      continue;
    tutorials_excluding_summary.emplace_back(tutorial);
  }

  return tutorials_excluding_summary;
}

}  // namespace

TutorialManagerImpl::TutorialManagerImpl(std::unique_ptr<TutorialStore> store,
                                         PrefService* prefs)
    : store_(std::move(store)), prefs_(prefs) {
  Initialize();
}

TutorialManagerImpl::~TutorialManagerImpl() = default;

void TutorialManagerImpl::Initialize() {
  store_->Initialize(base::BindOnce(&TutorialManagerImpl::OnInitCompleted,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void TutorialManagerImpl::GetTutorials(MultipleItemCallback callback) {
  if (!init_success_.has_value()) {
    MaybeCacheApiCall(base::BindOnce(&TutorialManagerImpl::GetTutorials,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(callback)));
    return;
  }

  // Find the data from cache. If the preferred locale is not set, use a default
  // locale value to show the tutorial promos. Users will be asked again to
  // confirm their language before the video starts.
  absl::optional<std::string> preferred_locale = GetPreferredLocale();
  std::string locale = preferred_locale.has_value()
                           ? preferred_locale.value()
                           : Config::GetDefaultPreferredLocale();
  if (tutorial_group_.has_value() && tutorial_group_->language == locale) {
    std::move(callback).Run(FilterTutorials(tutorial_group_->tutorials));
    return;
  }

  // The data doesn't exist in the cache. Probably the locale has changed. Load
  // it from the DB.
  store_->LoadEntries(
      {locale},
      base::BindOnce(&TutorialManagerImpl::OnTutorialsLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TutorialManagerImpl::GetTutorial(FeatureType feature_type,
                                      SingleItemCallback callback) {
  // Ensure that all the tutorials are already loaded.
  GetTutorials(base::BindOnce(&TutorialManagerImpl::RunSingleItemCallback,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::move(callback), feature_type));
}

void TutorialManagerImpl::RunSingleItemCallback(
    SingleItemCallback callback,
    FeatureType feature_type,
    std::vector<Tutorial> tutorials_excluding_summary) {
  if (!tutorial_group_.has_value()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  for (const Tutorial& tutorial : tutorial_group_->tutorials) {
    if (tutorial.feature == feature_type) {
      std::move(callback).Run(tutorial);
      return;
    }
  }

  std::move(callback).Run(absl::nullopt);
}

const std::vector<std::string>& TutorialManagerImpl::GetSupportedLanguages() {
  return supported_languages_;
}

const std::vector<std::string>&
TutorialManagerImpl::GetAvailableLanguagesForTutorial(
    FeatureType feature_type) {
  return languages_for_tutorials_[feature_type];
}

absl::optional<std::string> TutorialManagerImpl::GetPreferredLocale() {
  if (prefs_->HasPrefPath(kPreferredLocaleKey))
    return prefs_->GetString(kPreferredLocaleKey);
  return absl::nullopt;
}

void TutorialManagerImpl::SetPreferredLocale(const std::string& locale) {
  prefs_->SetString(kPreferredLocaleKey, locale);
}

void TutorialManagerImpl::OnInitCompleted(bool success) {
  if (!success)
    return;

  store_->LoadEntries({},
                      base::BindOnce(&TutorialManagerImpl::OnInitialDataLoaded,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void TutorialManagerImpl::OnInitialDataLoaded(
    bool success,
    std::unique_ptr<std::vector<TutorialGroup>> all_groups) {
  if (all_groups) {
    supported_languages_.clear();
    for (auto& tutorial_group : *all_groups) {
      supported_languages_.emplace_back(tutorial_group.language);
    }

    languages_for_tutorials_.clear();
    for (auto& tutorial_group : *all_groups) {
      for (auto& tutorial : tutorial_group.tutorials) {
        languages_for_tutorials_[tutorial.feature].emplace_back(
            tutorial_group.language);
      }
    }
  }

  init_success_ = success;

  // Flush all cached calls in FIFO sequence.
  while (!cached_api_calls_.empty()) {
    auto api_call = std::move(cached_api_calls_.front());
    cached_api_calls_.pop_front();
    std::move(api_call).Run();
  }
}

void TutorialManagerImpl::OnTutorialsLoaded(
    MultipleItemCallback callback,
    bool success,
    std::unique_ptr<std::vector<TutorialGroup>> loaded_groups) {
  if (!success || !loaded_groups || loaded_groups->empty()) {
    std::move(callback).Run(std::vector<Tutorial>());
    return;
  }

  if (!init_success_.has_value()) {
    MaybeCacheApiCall(base::BindOnce(
        &TutorialManagerImpl::OnTutorialsLoaded, weak_ptr_factory_.GetWeakPtr(),
        std::move(callback), success, std::move(loaded_groups)));
    return;
  }

  // We are loading tutorials only for the preferred locale.
  DCHECK(loaded_groups->size() == 1u);
  tutorial_group_ = loaded_groups->front();
  std::move(callback).Run(FilterTutorials(tutorial_group_->tutorials));
}

void TutorialManagerImpl::SaveGroups(
    std::unique_ptr<std::vector<TutorialGroup>> groups) {
  std::vector<std::string> new_locales;
  std::vector<std::pair<std::string, TutorialGroup>> key_entry_pairs;
  for (auto& group : *groups.get()) {
    new_locales.emplace_back(group.language);
    key_entry_pairs.emplace_back(std::make_pair(group.language, group));
  }

  // Remove the languages that don't exist in the new data.
  // TODO(shaktisahu): Maybe completely nuke the DB and save new data.
  std::vector<std::string> keys_to_delete;
  for (auto& old_language : supported_languages_) {
    if (std::find(new_locales.begin(), new_locales.end(), old_language) ==
        new_locales.end()) {
      keys_to_delete.emplace_back(old_language);
    }
  }

  store_->UpdateAll(key_entry_pairs, keys_to_delete,
                    base::BindOnce(&TutorialManagerImpl::OnInitCompleted,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void TutorialManagerImpl::MaybeCacheApiCall(base::OnceClosure api_call) {
  DCHECK(!init_success_.has_value())
      << "Only cache API calls before initialization.";
  cached_api_calls_.emplace_back(std::move(api_call));
}

}  // namespace video_tutorials
