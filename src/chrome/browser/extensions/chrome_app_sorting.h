// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_APP_SORTING_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_APP_SORTING_H_

#include <stddef.h>

#include <map>
#include <set>
#include <string>

#include "base/macros.h"
#include "components/sync/model/string_ordinal.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension_id.h"

namespace extensions {

class ChromeAppSorting : public AppSorting {
 public:
  explicit ChromeAppSorting(content::BrowserContext* browser_context);
  ~ChromeAppSorting() override;

  // AppSorting implementation:
  void FixNTPOrdinalCollisions() override;
  void EnsureValidOrdinals(
      const std::string& extension_id,
      const syncer::StringOrdinal& suggested_page) override;
  bool GetDefaultOrdinals(const std::string& extension_id,
                          syncer::StringOrdinal* page_ordinal,
                          syncer::StringOrdinal* app_launch_ordinal) override;
  void OnExtensionMoved(const std::string& moved_extension_id,
                        const std::string& predecessor_extension_id,
                        const std::string& successor_extension_id) override;
  syncer::StringOrdinal GetAppLaunchOrdinal(
      const std::string& extension_id) const override;
  void SetAppLaunchOrdinal(
      const std::string& extension_id,
      const syncer::StringOrdinal& new_app_launch_ordinal) override;
  syncer::StringOrdinal CreateFirstAppLaunchOrdinal(
      const syncer::StringOrdinal& page_ordinal) const override;
  syncer::StringOrdinal CreateNextAppLaunchOrdinal(
      const syncer::StringOrdinal& page_ordinal) const override;
  syncer::StringOrdinal CreateFirstAppPageOrdinal() const override;
  syncer::StringOrdinal GetNaturalAppPageOrdinal() const override;
  syncer::StringOrdinal GetPageOrdinal(
      const std::string& extension_id) const override;
  void SetPageOrdinal(const std::string& extension_id,
                      const syncer::StringOrdinal& new_page_ordinal) override;
  void ClearOrdinals(const std::string& extension_id) override;
  int PageStringOrdinalAsInteger(
      const syncer::StringOrdinal& page_ordinal) const override;
  syncer::StringOrdinal PageIntegerAsStringOrdinal(size_t page_index) override;
  void SetExtensionVisible(const std::string& extension_id,
                           bool visible) override;

 private:
  // The StringOrdinal is the app launch ordinal and the string is the extension
  // id.
  typedef std::multimap<
      syncer::StringOrdinal, std::string,
    syncer::StringOrdinal::LessThanFn> AppLaunchOrdinalMap;
  // The StringOrdinal is the page ordinal and the AppLaunchOrdinalMap is the
  // contents of that page.
  typedef std::map<
      syncer::StringOrdinal, AppLaunchOrdinalMap,
    syncer::StringOrdinal::LessThanFn> PageOrdinalMap;

  // Unit tests.
  friend class ChromeAppSortingDefaultOrdinalsBase;
  friend class ChromeAppSortingGetMinOrMaxAppLaunchOrdinalsOnPage;
  friend class ChromeAppSortingInitialize;
  friend class ChromeAppSortingInitializeWithNoApps;
  friend class ChromeAppSortingPageOrdinalMapping;
  friend class ChromeAppSortingSetExtensionVisible;

  // An enum used by GetMinOrMaxAppLaunchOrdinalsOnPage to specify which
  // value should be returned.
  enum AppLaunchOrdinalReturn {MIN_ORDINAL, MAX_ORDINAL};

  // Maps an app id to its ordinals.
  struct AppOrdinals {
    AppOrdinals();
    AppOrdinals(const AppOrdinals& other);
    ~AppOrdinals();

    syncer::StringOrdinal page_ordinal;
    syncer::StringOrdinal app_launch_ordinal;
  };
  typedef std::map<std::string, AppOrdinals> AppOrdinalsMap;

  // This function returns the lowest ordinal on |page_ordinal| if
  // |return_value| == AppLaunchOrdinalReturn::MIN_ORDINAL, otherwise it returns
  // the largest ordinal on |page_ordinal|. If there are no apps on the page
  // then an invalid StringOrdinal is returned. It is an error to call this
  // function with an invalid |page_ordinal|.
  syncer::StringOrdinal GetMinOrMaxAppLaunchOrdinalsOnPage(
      const syncer::StringOrdinal& page_ordinal,
      AppLaunchOrdinalReturn return_type) const;

  // Initialize the |page_ordinal_map_| with the page ordinals used by the
  // given extensions.
  void InitializePageOrdinalMap(
      const extensions::ExtensionIdList& extension_ids);

  // Migrates the app launcher and page index values.
  void MigrateAppIndex(
      const extensions::ExtensionIdList& extension_ids);

  // Called to add a new mapping value for |extension_id| with a page ordinal
  // of |page_ordinal| and a app launch ordinal of |app_launch_ordinal|. This
  // works with valid and invalid StringOrdinals.
  void AddOrdinalMapping(const std::string& extension_id,
                         const syncer::StringOrdinal& page_ordinal,
                         const syncer::StringOrdinal& app_launch_ordinal);

  // Ensures |ntp_ordinal_map_| is of |minimum_size| number of entries.
  void CreateOrdinalsIfNecessary(size_t minimum_size);

  // Removes the mapping for |extension_id| with a page ordinal of
  // |page_ordinal| and a app launch ordinal of |app_launch_ordinal|. If there
  // is not matching map, nothing happens. This works with valid and invalid
  // StringOrdinals.
  void RemoveOrdinalMapping(const std::string& extension_id,
                            const syncer::StringOrdinal& page_ordinal,
                            const syncer::StringOrdinal& app_launch_ordinal);

  // Syncs the extension if needed. It is an error to call this if the
  // extension is not an application.
  void SyncIfNeeded(const std::string& extension_id);

  // Creates the default ordinals.
  void CreateDefaultOrdinals();

  // Returns |app_launch_ordinal| if it has no collision in the page specified
  // by |page_ordinal|. Otherwise, returns an ordinal after |app_launch_ordinal|
  // that has no conflict.
  syncer::StringOrdinal ResolveCollision(
      const syncer::StringOrdinal& page_ordinal,
      const syncer::StringOrdinal& app_launch_ordinal) const;

  // Returns the number of items in |m| visible on the new tab page.
  size_t CountItemsVisibleOnNtp(const AppLaunchOrdinalMap& m) const;

  content::BrowserContext* browser_context_;

  // A map of all the StringOrdinal page ordinals mapping to the collections of
  // app launch ordinals that exist on that page. This is used for mapping
  // StringOrdinals to their Integer equivalent as well as quick lookup of the
  // any collision of on the NTP (icons with the same page and same app launch
  // ordinals). The possiblity of collisions means that a multimap must be used
  // (although the collisions must all be resolved once all the syncing is
  // done).
  PageOrdinalMap ntp_ordinal_map_;

  // Defines the default ordinals.
  AppOrdinalsMap default_ordinals_;

  // Used to construct the default ordinals once when needed instead of on
  // construction when the app order may not have been determined.
  bool default_ordinals_created_;

  // The set of extensions that don't appear in the new tab page.
  std::set<std::string> ntp_hidden_extensions_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAppSorting);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_APP_SORTING_H_
