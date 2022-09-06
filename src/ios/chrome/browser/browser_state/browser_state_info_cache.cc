// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browser_state/browser_state_info_cache.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ios/chrome/browser/browser_state/browser_state_info_cache_observer.h"
#include "ios/chrome/browser/pref_names.h"

namespace {
const char kGAIAIdKey[] = "gaia_id";
const char kIsAuthErrorKey[] = "is_auth_error";
const char kUserNameKey[] = "user_name";
}

BrowserStateInfoCache::BrowserStateInfoCache(
    PrefService* prefs,
    const base::FilePath& user_data_dir)
    : prefs_(prefs), user_data_dir_(user_data_dir) {
  // Populate the cache
  DictionaryPrefUpdate update(prefs_, prefs::kBrowserStateInfoCache);
  base::Value* cache = update.Get();
  for (const auto it : cache->DictItems()) {
    AddBrowserStateCacheKey(it.first);
  }
}

BrowserStateInfoCache::~BrowserStateInfoCache() {}

void BrowserStateInfoCache::AddBrowserState(
    const base::FilePath& browser_state_path,
    const std::string& gaia_id,
    const std::u16string& user_name) {
  std::string key = CacheKeyFromBrowserStatePath(browser_state_path);
  DictionaryPrefUpdate update(prefs_, prefs::kBrowserStateInfoCache);
  base::Value* cache = update.Get();

  base::Value info(base::Value::Type::DICTIONARY);
  info.SetStringKey(kGAIAIdKey, gaia_id);
  info.SetStringKey(kUserNameKey, user_name);
  cache->SetKey(key, std::move(info));
  AddBrowserStateCacheKey(key);

  for (auto& observer : observer_list_)
    observer.OnBrowserStateAdded(browser_state_path);
}

void BrowserStateInfoCache::AddObserver(
    BrowserStateInfoCacheObserver* observer) {
  observer_list_.AddObserver(observer);
}

void BrowserStateInfoCache::RemoveObserver(
    BrowserStateInfoCacheObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void BrowserStateInfoCache::RemoveBrowserState(
    const base::FilePath& browser_state_path) {
  size_t browser_state_index =
      GetIndexOfBrowserStateWithPath(browser_state_path);
  if (browser_state_index == std::string::npos) {
    NOTREACHED();
    return;
  }
  DictionaryPrefUpdate update(prefs_, prefs::kBrowserStateInfoCache);
  base::Value* cache = update.Get();
  std::string key = CacheKeyFromBrowserStatePath(browser_state_path);
  cache->RemoveKey(key);
  sorted_keys_.erase(std::find(sorted_keys_.begin(), sorted_keys_.end(), key));

  for (auto& observer : observer_list_)
    observer.OnBrowserStateWasRemoved(browser_state_path);
}

size_t BrowserStateInfoCache::GetNumberOfBrowserStates() const {
  return sorted_keys_.size();
}

size_t BrowserStateInfoCache::GetIndexOfBrowserStateWithPath(
    const base::FilePath& browser_state_path) const {
  if (browser_state_path.DirName() != user_data_dir_)
    return std::string::npos;
  std::string search_key = CacheKeyFromBrowserStatePath(browser_state_path);
  for (size_t i = 0; i < sorted_keys_.size(); ++i) {
    if (sorted_keys_[i] == search_key)
      return i;
  }
  return std::string::npos;
}

std::u16string BrowserStateInfoCache::GetUserNameOfBrowserStateAtIndex(
    size_t index) const {
  const base::Value* value = GetInfoForBrowserStateAtIndex(index);
  const std::string* user_name = value->FindStringKey(kUserNameKey);
  return user_name ? base::ASCIIToUTF16(*user_name) : std::u16string();
}

base::FilePath BrowserStateInfoCache::GetPathOfBrowserStateAtIndex(
    size_t index) const {
  return user_data_dir_.AppendASCII(sorted_keys_[index]);
}

std::string BrowserStateInfoCache::GetGAIAIdOfBrowserStateAtIndex(
    size_t index) const {
  const base::Value* value = GetInfoForBrowserStateAtIndex(index);
  const std::string* gaia_id = value->FindStringKey(kGAIAIdKey);
  return gaia_id ? *gaia_id : std::string();
}

bool BrowserStateInfoCache::BrowserStateIsAuthenticatedAtIndex(
    size_t index) const {
  // The browser state is authenticated if the gaia_id of the info is not empty.
  // If it is empty, also check if the user name is not empty.  This latter
  // check is needed in case the browser state has not been loaded yet and the
  // gaia_id property has not yet been written.
  return !GetGAIAIdOfBrowserStateAtIndex(index).empty() ||
         !GetUserNameOfBrowserStateAtIndex(index).empty();
}

bool BrowserStateInfoCache::BrowserStateIsAuthErrorAtIndex(size_t index) const {
  return GetInfoForBrowserStateAtIndex(index)
      ->FindBoolPath(kIsAuthErrorKey)
      .value_or(false);
}

void BrowserStateInfoCache::SetAuthInfoOfBrowserStateAtIndex(
    size_t index,
    const std::string& gaia_id,
    const std::u16string& user_name) {
  // If both gaia_id and username are unchanged, abort early.
  if (gaia_id == GetGAIAIdOfBrowserStateAtIndex(index) &&
      user_name == GetUserNameOfBrowserStateAtIndex(index)) {
    return;
  }

  base::Value info = GetInfoForBrowserStateAtIndex(index)->Clone();
  info.SetKey(kGAIAIdKey, base::Value(gaia_id));
  info.SetKey(kUserNameKey, base::Value(user_name));
  SetInfoForBrowserStateAtIndex(index, std::move(info));
}

void BrowserStateInfoCache::SetBrowserStateIsAuthErrorAtIndex(size_t index,
                                                              bool value) {
  if (value == BrowserStateIsAuthErrorAtIndex(index))
    return;

  base::Value info = GetInfoForBrowserStateAtIndex(index)->Clone();
  info.SetKey(kIsAuthErrorKey, base::Value(value));
  SetInfoForBrowserStateAtIndex(index, std::move(info));
}

const base::FilePath& BrowserStateInfoCache::GetUserDataDir() const {
  return user_data_dir_;
}

// static
void BrowserStateInfoCache::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kBrowserStateInfoCache);
}

const base::Value* BrowserStateInfoCache::GetInfoForBrowserStateAtIndex(
    size_t index) const {
  DCHECK_LT(index, GetNumberOfBrowserStates());
  return prefs_->GetDictionary(prefs::kBrowserStateInfoCache)
      ->FindDictKey(sorted_keys_[index]);
}

void BrowserStateInfoCache::SetInfoForBrowserStateAtIndex(size_t index,
                                                          base::Value info) {
  DictionaryPrefUpdate update(prefs_, prefs::kBrowserStateInfoCache);
  base::Value* cache = update.Get();
  cache->SetKey(sorted_keys_[index], std::move(info));
}

std::string BrowserStateInfoCache::CacheKeyFromBrowserStatePath(
    const base::FilePath& browser_state_path) const {
  DCHECK(user_data_dir_ == browser_state_path.DirName());
  base::FilePath base_name = browser_state_path.BaseName();
  return base_name.MaybeAsASCII();
}

void BrowserStateInfoCache::AddBrowserStateCacheKey(const std::string& key) {
  sorted_keys_.insert(
      std::upper_bound(sorted_keys_.begin(), sorted_keys_.end(), key), key);
}
