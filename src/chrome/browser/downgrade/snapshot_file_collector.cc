// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/downgrade/snapshot_file_collector.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/common/chrome_constants.h"
#include "components/autofill/core/browser/payments/strike_database.h"
#include "components/bookmarks/common/bookmark_constants.h"
#include "components/history/core/browser/history_constants.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/sessions/core/session_constants.h"
#include "components/webdata/common/webdata_constants.h"
#include "content/public/browser/browsing_data_remover.h"

#if defined(OS_WIN)
#include "chrome/browser/profiles/profile_shortcut_manager_win.h"
#include "chrome/browser/web_applications/chrome_pwa_launcher/last_browser_file_util.h"
#endif

namespace downgrade {

SnapshotItemDetails::SnapshotItemDetails(base::FilePath path,
                                         ItemType item_type,
                                         int data_types)
    : path(std::move(path)),
      is_directory(item_type == ItemType::kDirectory),
      data_types(data_types) {}

// Returns a list of items to snapshot that should be directly under the user
// data  directory.
std::vector<SnapshotItemDetails> CollectUserDataItems() {
  std::vector<SnapshotItemDetails> user_data_items{
      SnapshotItemDetails(base::FilePath(chrome::kLocalStateFilename),
                          SnapshotItemDetails::ItemType::kFile, 0),
      SnapshotItemDetails(base::FilePath(profiles::kHighResAvatarFolderName),
                          SnapshotItemDetails::ItemType::kDirectory, 0)};
#if defined(OS_WIN)
  user_data_items.emplace_back(base::FilePath(web_app::kLastBrowserFilename),
                               SnapshotItemDetails::ItemType::kFile, 0);
#endif  // defined(OS_WIN)
  return user_data_items;
}

// Returns a list of items to snapshot that should be under a profile directory.
std::vector<SnapshotItemDetails> CollectProfileItems() {
  // Data mask to delete the pref files if any of the following types is
  // deleted. When cookies are deleted, the kZeroSuggestCachedResults pref has
  // to be reset. When history and isolated origins are deleted, the
  // kPrefLastLaunchTime and kUserTriggeredIsolatedOrigins prefs have to be
  // reset. When data type content is deleted, blacklisted sites are deleted
  // from the translation prefs.
  int pref_data_type =
      content::BrowsingDataRemover::DATA_TYPE_COOKIES |
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_ISOLATED_ORIGINS |
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY |
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_CONTENT_SETTINGS;
  std::vector<SnapshotItemDetails> profile_items{
      // General Profile files
      SnapshotItemDetails(base::FilePath(chrome::kPreferencesFilename),
                          SnapshotItemDetails::ItemType::kFile, pref_data_type),
      SnapshotItemDetails(base::FilePath(chrome::kSecurePreferencesFilename),
                          SnapshotItemDetails::ItemType::kFile, pref_data_type),
      // History files
      SnapshotItemDetails(base::FilePath(history::kHistoryFilename),
                          SnapshotItemDetails::ItemType::kFile,
                          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY),
      SnapshotItemDetails(base::FilePath(history::kFaviconsFilename),
                          SnapshotItemDetails::ItemType::kFile,
                          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY),
      SnapshotItemDetails(base::FilePath(history::kTopSitesFilename),
                          SnapshotItemDetails::ItemType::kFile,
                          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY),
      // Bookmarks
      SnapshotItemDetails(
          base::FilePath(bookmarks::kBookmarksFileName),
          SnapshotItemDetails::ItemType::kFile,
          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_BOOKMARKS),
      // Tab Restore and sessions
      SnapshotItemDetails(base::FilePath(sessions::kCurrentTabSessionFileName),
                          SnapshotItemDetails::ItemType::kFile,
                          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY),
      SnapshotItemDetails(base::FilePath(sessions::kCurrentSessionFileName),
                          SnapshotItemDetails::ItemType::kFile,
                          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY),
      // Sign-in state
      SnapshotItemDetails(base::FilePath(profiles::kGAIAPictureFileName),
                          SnapshotItemDetails::ItemType::kFile, 0),
      // Password / Autofill
      SnapshotItemDetails(
          base::FilePath(password_manager::kAffiliationDatabaseFileName),
          SnapshotItemDetails::ItemType::kFile,
          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS |
              ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA),
      SnapshotItemDetails(
          base::FilePath(password_manager::kLoginDataForProfileFileName),
          SnapshotItemDetails::ItemType::kFile,
          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS |
              ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA),
      SnapshotItemDetails(
          base::FilePath(password_manager::kLoginDataForAccountFileName),
          SnapshotItemDetails::ItemType::kFile,
          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS |
              ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA),
      SnapshotItemDetails(
          base::FilePath(kWebDataFilename),
          SnapshotItemDetails::ItemType::kFile,
          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS |
              ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA),
      SnapshotItemDetails(
          base::FilePath(autofill::kStrikeDatabaseFileName),
          SnapshotItemDetails::ItemType::kDirectory,
          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS |
              ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA),
      // Cookies
      SnapshotItemDetails(base::FilePath(chrome::kCookieFilename),
                          SnapshotItemDetails::ItemType::kFile,
                          content::BrowsingDataRemover::DATA_TYPE_COOKIES)};

#if defined(OS_WIN)
  // Sign-in state
  profile_items.emplace_back(base::FilePath(profiles::kProfileIconFileName),
                             SnapshotItemDetails::ItemType::kFile, 0);
#endif  // defined(OS_WIN)
  return profile_items;
}

}  // namespace downgrade
