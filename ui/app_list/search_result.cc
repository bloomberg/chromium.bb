// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/search_result.h"

#include "ui/app_list/search_result_observer.h"

namespace app_list {

SearchResult::SearchResult() {
}

SearchResult::~SearchResult() {
}

void SearchResult::SetIcon(const gfx::ImageSkia& icon) {
  icon_ = icon;
  FOR_EACH_OBSERVER(SearchResultObserver,
                    observers_,
                    OnIconChanged());
}

void SearchResult::AddObserver(SearchResultObserver* observer) {
  observers_.AddObserver(observer);
}

void SearchResult::RemoveObserver(SearchResultObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace app_list
