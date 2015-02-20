// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/all_apps_tile_item_view.h"

#include "base/metrics/histogram_macros.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace app_list {

AllAppsTileItemView::AllAppsTileItemView(ContentsView* contents_view,
                                         AppListItemList* item_list)
    : contents_view_(contents_view), folder_image_(item_list) {
  SetTitle(l10n_util::GetStringUTF16(IDS_APP_LIST_ALL_APPS));
  folder_image_.AddObserver(this);
}

AllAppsTileItemView::~AllAppsTileItemView() {
  folder_image_.RemoveObserver(this);
}

void AllAppsTileItemView::UpdateIcon() {
  folder_image_.UpdateIcon();
}

void AllAppsTileItemView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  UMA_HISTOGRAM_ENUMERATION(kPageOpenedHistogram, AppListModel::STATE_APPS,
                            AppListModel::STATE_LAST);

  contents_view_->SetActivePage(
      contents_view_->GetPageIndexForState(AppListModel::STATE_APPS));
}

void AllAppsTileItemView::OnFolderImageUpdated() {
  SetIcon(folder_image_.icon());
}

}  // namespace app_list
