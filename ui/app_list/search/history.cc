// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/search/history.h"

#include <stddef.h>

#include "ash/app_list/model/search/tokenized_string.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/app_list/search/history_data.h"
#include "ui/app_list/search/history_data_store.h"

namespace app_list {

namespace {

// Normalize the given string by joining all its tokens with a space.
std::string NormalizeString(const std::string& utf8) {
  TokenizedString tokenized(base::UTF8ToUTF16(utf8));
  return base::UTF16ToUTF8(
      base::JoinString(tokenized.tokens(), base::ASCIIToUTF16(" ")));
}

}  // namespace

History::History(scoped_refptr<HistoryDataStore> store)
    : store_(store), data_loaded_(false) {
  const size_t kMaxQueryEntries = 1000;
  const size_t kMaxSecondaryQueries = 5;

  data_.reset(
      new HistoryData(store_.get(), kMaxQueryEntries, kMaxSecondaryQueries));
  data_->AddObserver(this);
}

History::~History() {
  data_->RemoveObserver(this);
}

bool History::IsReady() const {
  return data_loaded_;
}

void History::AddLaunchEvent(const std::string& query,
                             const std::string& result_id) {
  DCHECK(IsReady());
  data_->Add(NormalizeString(query), result_id);
}

std::unique_ptr<KnownResults> History::GetKnownResults(
    const std::string& query) const {
  DCHECK(IsReady());
  return data_->GetKnownResults(NormalizeString(query));
}

void History::OnHistoryDataLoadedFromStore() {
  data_loaded_ = true;
}

}  // namespace app_list
