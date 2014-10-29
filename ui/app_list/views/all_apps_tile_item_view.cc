// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/all_apps_tile_item_view.h"

#include "ui/app_list/views/contents_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace app_list {

AllAppsTileItemView::AllAppsTileItemView(ContentsView* contents_view)
    : contents_view_(contents_view) {
  SetTitle(l10n_util::GetStringUTF16(IDS_APP_LIST_ALL_APPS));
  // TODO(mgiuca): Set the button's icon.
}

AllAppsTileItemView::~AllAppsTileItemView() {
}

void AllAppsTileItemView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  contents_view_->SetActivePage(
      contents_view_->GetPageIndexForState(AppListModel::STATE_APPS));
}

}  // namespace app_list
