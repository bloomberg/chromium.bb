// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/all_apps_tile_item_view.h"

#include "base/metrics/histogram_macros.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/strings/grit/ui_strings.h"

namespace app_list {

AllAppsTileItemView::AllAppsTileItemView(ContentsView* contents_view)
    : contents_view_(contents_view) {
  SetTitle(l10n_util::GetStringUTF16(IDS_APP_LIST_ALL_APPS));
  SetHoverStyle(TileItemView::HOVER_STYLE_ANIMATE_SHADOW);
  UpdateIcon();
}

AllAppsTileItemView::~AllAppsTileItemView() {
}

void AllAppsTileItemView::UpdateIcon() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SetIcon(*rb.GetImageNamed(IDR_ALL_APPS_DROP_DOWN).ToImageSkia());
}

void AllAppsTileItemView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  UMA_HISTOGRAM_ENUMERATION(kPageOpenedHistogram, AppListModel::STATE_APPS,
                            AppListModel::STATE_LAST);

  contents_view_->SetActiveState(AppListModel::STATE_APPS);
}

}  // namespace app_list
