// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/pinned_tab_codec.h"

#include <stddef.h>

#include <utility>

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

using content::NavigationEntry;

namespace {

// Key used in dictionaries for the url.
const char kURL[] = "url";

// Returns a Value representing the supplied StartupTab.
base::Value EncodeTab(const GURL& url) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringPath(kURL, url.spec());
  return dict;
}

// Encodes all the pinned tabs from |browser| into |serialized_tabs|.
void EncodePinnedTabs(Browser* browser, base::Value* serialized_tabs) {
  DCHECK(serialized_tabs->is_list());

  TabStripModel* tab_model = browser->tab_strip_model();
  for (int i = 0; i < tab_model->count() && tab_model->IsTabPinned(i); ++i) {
    content::WebContents* web_contents = tab_model->GetWebContentsAt(i);
    NavigationEntry* entry =
        web_contents->GetController().GetLastCommittedEntry();
    if (entry)
      serialized_tabs->Append(EncodeTab(entry->GetURL()));
  }
}

// Decodes the previously written values in |value| to |tab|, returning true
// on success.
absl::optional<StartupTab> DecodeTab(const base::Value& value) {
  const std::string* const url_string = value.FindStringPath(kURL);
  return url_string ? absl::make_optional(StartupTab(GURL(*url_string),
                                                     StartupTab::Type::kPinned))
                    : absl::nullopt;
}

}  // namespace

// static
void PinnedTabCodec::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kPinnedTabs);
}

// static
void PinnedTabCodec::WritePinnedTabs(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  if (!prefs)
    return;

  base::Value values(base::Value::Type::LIST);
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->is_type_normal() && browser->profile() == profile) {
      EncodePinnedTabs(browser, &values);
    }
  }
  prefs->Set(prefs::kPinnedTabs, values);
}

// static
void PinnedTabCodec::WritePinnedTabs(Profile* profile,
                                     const StartupTabs& tabs) {
  PrefService* prefs = profile->GetPrefs();
  if (!prefs)
    return;

  ListPrefUpdate update(prefs, prefs::kPinnedTabs);
  base::Value* values = update.Get();
  values->ClearList();
  for (const auto& tab : tabs)
    values->Append(EncodeTab(tab.url));
}

// static
StartupTabs PinnedTabCodec::ReadPinnedTabs(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  if (!prefs)
    return {};
  return ReadPinnedTabs(prefs->GetList(prefs::kPinnedTabs));
}

// static
StartupTabs PinnedTabCodec::ReadPinnedTabs(const base::Value* value) {
  StartupTabs results;

  if (!value->is_list())
    return results;

  for (const auto& serialized_tab : value->GetList()) {
    if (!serialized_tab.is_dict())
      continue;
    absl::optional<StartupTab> tab = DecodeTab(serialized_tab);
    if (tab.has_value())
      results.push_back(tab.value());
  }

  return results;
}
