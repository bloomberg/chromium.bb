// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/crostini/crostini_repository_search_provider.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "base/bind.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/crostini/crostini_repository_search_result.h"

namespace app_list {

CrostiniRepositorySearchProvider::CrostiniRepositorySearchProvider(
    Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {}

CrostiniRepositorySearchProvider::~CrostiniRepositorySearchProvider() = default;

void CrostiniRepositorySearchProvider::OnStart(
    const std::vector<std::string>& app_names) {
  SearchProvider::Results new_results;
  new_results.reserve(app_names.size());
  for (auto& app_name : app_names) {
    new_results.emplace_back(
        std::make_unique<CrostiniRepositorySearchResult>(profile_, app_name));
    // Todo(https://crbug.com/921429): Improve relevance logic, this will likely
    // be implemented in garcon then piped to Chrome
    new_results.back()->set_relevance(static_cast<float>(query_.size()) /
                                      app_name.size());
  }
  SwapResults(&new_results);
}

void CrostiniRepositorySearchProvider::Start(const base::string16& query) {
  // Only perform search when Crostini is running.
  if (!crostini::IsCrostiniRunning(profile_)) {
    return;
  }
  if (query.empty()) {
    ClearResults();
    return;
  }
  query_ = base::UTF16ToUTF8(query);

  crostini::CrostiniManager::GetForProfile(profile_)->SearchApp(
      crostini::kCrostiniDefaultVmName, crostini::kCrostiniDefaultContainerName,
      query_,
      base::BindOnce(&CrostiniRepositorySearchProvider::OnStart,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace app_list
