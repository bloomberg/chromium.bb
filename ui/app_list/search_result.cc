// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/search_result.h"

#include "ui/app_list/search_result_observer.h"

namespace app_list {

SearchResult::Action::Action(const gfx::ImageSkia& base_image,
                             const gfx::ImageSkia& hover_image,
                             const gfx::ImageSkia& pressed_image,
                             const base::string16& tooltip_text)
    : base_image(base_image),
      hover_image(hover_image),
      pressed_image(pressed_image),
      tooltip_text(tooltip_text) {}

SearchResult::Action::Action(const base::string16& label_text,
                             const base::string16& tooltip_text)
    : tooltip_text(tooltip_text),
      label_text(label_text) {}

SearchResult::Action::~Action() {}

SearchResult::SearchResult()
    : relevance_(0), is_installing_(false), percent_downloaded_(0) {
}

SearchResult::~SearchResult() {}

void SearchResult::SetIcon(const gfx::ImageSkia& icon) {
  icon_ = icon;
  FOR_EACH_OBSERVER(SearchResultObserver,
                    observers_,
                    OnIconChanged());
}

void SearchResult::SetActions(const Actions& sets) {
  actions_ = sets;
  FOR_EACH_OBSERVER(SearchResultObserver,
                    observers_,
                    OnActionsChanged());
}

void SearchResult::SetIsInstalling(bool is_installing) {
  if (is_installing_ == is_installing)
    return;

  is_installing_ = is_installing;
  FOR_EACH_OBSERVER(SearchResultObserver,
                    observers_,
                    OnIsInstallingChanged());
}

void SearchResult::SetPercentDownloaded(int percent_downloaded) {
  if (percent_downloaded_ == percent_downloaded)
    return;

  percent_downloaded_ = percent_downloaded;
  FOR_EACH_OBSERVER(SearchResultObserver,
                    observers_,
                    OnPercentDownloadedChanged());
}

void SearchResult::NotifyItemInstalled() {
  FOR_EACH_OBSERVER(SearchResultObserver, observers_, OnItemInstalled());
}

void SearchResult::NotifyItemUninstalled() {
  FOR_EACH_OBSERVER(SearchResultObserver, observers_, OnItemUninstalled());
}

void SearchResult::AddObserver(SearchResultObserver* observer) {
  observers_.AddObserver(observer);
}

void SearchResult::RemoveObserver(SearchResultObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SearchResult::Open(int event_flags) {
}

void SearchResult::InvokeAction(int action_index, int event_flags) {
}

ui::MenuModel* SearchResult::GetContextMenuModel() {
  return NULL;
}

}  // namespace app_list
