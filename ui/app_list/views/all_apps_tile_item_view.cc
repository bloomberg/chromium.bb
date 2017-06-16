// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/all_apps_tile_item_view.h"

#include "base/metrics/histogram_macros.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/resources/grit/app_list_resources.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/strings/grit/ui_strings.h"

namespace app_list {

namespace {

class AllAppsImageSource : public gfx::CanvasImageSource {
 public:
  AllAppsImageSource(const gfx::ImageSkia& icon, const gfx::Size& size)
      : gfx::CanvasImageSource(size, false), icon_(icon) {}
  ~AllAppsImageSource() override {}

 private:
  // gfx::CanvasImageSource override:
  void Draw(gfx::Canvas* canvas) override {
    canvas->DrawImageInt(icon_, (size().width() - icon_.size().width()) / 2,
                         (size().height() - icon_.size().height()) / 2);
  }

  gfx::ImageSkia icon_;

  DISALLOW_COPY_AND_ASSIGN(AllAppsImageSource);
};

}  // namespace

AllAppsTileItemView::AllAppsTileItemView(ContentsView* contents_view,
                                         AppListView* app_list_view)
    : contents_view_(contents_view), app_list_view_(app_list_view) {
  SetTitle(l10n_util::GetStringUTF16(IDS_APP_LIST_ALL_APPS));
  SetHoverStyle(TileItemView::HOVER_STYLE_ANIMATE_SHADOW);
  UpdateIcon();
}

AllAppsTileItemView::~AllAppsTileItemView() {
}

void AllAppsTileItemView::UpdateIcon() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Size canvas_size = gfx::Size(kTileIconSize, kTileIconSize);
  AllAppsImageSource* source = new AllAppsImageSource(
      *rb.GetImageNamed(IDR_ALL_APPS_DROP_DOWN).ToImageSkia(), canvas_size);
  gfx::ImageSkia image(source, canvas_size);

  SetIcon(image);
}

void AllAppsTileItemView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  UMA_HISTOGRAM_ENUMERATION(kPageOpenedHistogram, AppListModel::STATE_APPS,
                            AppListModel::STATE_LAST);

  contents_view_->SetActiveState(AppListModel::STATE_APPS);
  if (features::IsFullscreenAppListEnabled())
    app_list_view_->SetState(AppListView::FULLSCREEN);
}

}  // namespace app_list
