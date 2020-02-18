// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/fake_suggestions_provider.h"

#include <utility>
#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace offline_pages {

FakeSuggestionsProvider::FakeSuggestionsProvider() = default;

FakeSuggestionsProvider::~FakeSuggestionsProvider() = default;

void FakeSuggestionsProvider::SetSuggestions(
    std::vector<PrefetchSuggestion> suggestions) {
  suggestions_ = std::move(suggestions);
}

void FakeSuggestionsProvider::ClearViews() {
  report_article_list_viewed_count_ = 0;
  article_views_.clear();
}

void FakeSuggestionsProvider::GetCurrentArticleSuggestions(
    SuggestionCallback suggestions_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(suggestions_callback), suggestions_));
}

void FakeSuggestionsProvider::ReportArticleListViewed() {
  report_article_list_viewed_count_++;
}

void FakeSuggestionsProvider::ReportArticleViewed(GURL article_url) {
  article_views_.push_back(article_url);
}

}  // namespace offline_pages
