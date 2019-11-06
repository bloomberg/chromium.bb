// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/pref_util.h"

#include <memory>

#include "base/values.h"
#include "components/prefs/pref_service.h"

namespace ntp_snippets {
namespace prefs {

std::set<std::string> ReadDismissedIDsFromPrefs(const PrefService& pref_service,
                                                const std::string& pref_name) {
  std::set<std::string> dismissed_ids;
  const base::ListValue* list = pref_service.GetList(pref_name);
  for (const base::Value& value : *list) {
    std::string dismissed_id;
    bool success = value.GetAsString(&dismissed_id);
    DCHECK(success) << "Failed to parse dismissed id from prefs param "
                    << pref_name << " into string.";
    dismissed_ids.insert(dismissed_id);
  }
  return dismissed_ids;
}

void StoreDismissedIDsToPrefs(PrefService* pref_service,
                              const std::string& pref_name,
                              const std::set<std::string>& dismissed_ids) {
  base::ListValue list;
  for (const std::string& dismissed_id : dismissed_ids) {
    list.AppendString(dismissed_id);
  }
  pref_service->Set(pref_name, list);
}

}  // namespace prefs
}  // namespace ntp_snippets
