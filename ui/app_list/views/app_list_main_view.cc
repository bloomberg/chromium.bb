// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_main_view.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/views/app_list_item_view.h"
#include "ui/app_list/views/contents_switcher_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace app_list {

namespace {

// Inner padding space in pixels of bubble contents.
const int kInnerPadding = 1;

// The maximum allowed time to wait for icon loading in milliseconds.
const int kMaxIconLoadingWaitTimeInMs = 50;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AppListMainView::IconLoader

class AppListMainView::IconLoader : public AppListItemObserver {
 public:
  IconLoader(AppListMainView* owner,
             AppListItem* item,
             float scale)
      : owner_(owner),
        item_(item) {
    item_->AddObserver(this);

    // Triggers icon loading for given |scale_factor|.
    item_->icon().GetRepresentation(scale);
  }

  virtual ~IconLoader() {
    item_->RemoveObserver(this);
  }

 private:
  // AppListItemObserver overrides:
  virtual void ItemIconChanged() OVERRIDE {
    owner_->OnItemIconLoaded(this);
    // Note that IconLoader is released here.
  }
  virtual void ItemNameChanged() OVERRIDE {}
  virtual void ItemHighlightedChanged() OVERRIDE {}
  virtual void ItemIsInstallingChanged() OVERRIDE {}
  virtual void ItemPercentDownloadedChanged() OVERRIDE {}

  AppListMainView* owner_;
  AppListItem* item_;

  DISALLOW_COPY_AND_ASSIGN(IconLoader);
};

////////////////////////////////////////////////////////////////////////////////
// AppListMainView:

AppListMainView::AppListMainView(AppListViewDelegate* delegate,
                                 PaginationModel* pagination_model,
                                 gfx::NativeView parent)
    : delegate_(delegate),
      pagination_model_(pagination_model),
      model_(delegate->GetModel()),
      search_box_view_(NULL),
      contents_view_(NULL),
      weak_ptr_factory_(this) {
  // Starts icon loading early.
  PreloadIcons(parent);

  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
                                        kInnerPadding,
                                        kInnerPadding,
                                        kInnerPadding));

  search_box_view_ = new SearchBoxView(this, delegate);
  AddChildView(search_box_view_);
  AddContentsView();
  if (app_list::switches::IsExperimentalAppListEnabled())
    AddChildView(new ContentsSwitcherView(contents_view_));
}

void AppListMainView::AddContentsView() {
  contents_view_ = new ContentsView(
      this, pagination_model_, model_, delegate_);
  AddChildView(contents_view_);

  search_box_view_->set_contents_view(contents_view_);

#if defined(USE_AURA)
  contents_view_->SetPaintToLayer(true);
  contents_view_->SetFillsBoundsOpaquely(false);
  contents_view_->layer()->SetMasksToBounds(true);
#endif
}

AppListMainView::~AppListMainView() {
  pending_icon_loaders_.clear();
}

void AppListMainView::ShowAppListWhenReady() {
  if (pending_icon_loaders_.empty()) {
    icon_loading_wait_timer_.Stop();
    GetWidget()->Show();
    return;
  }

  if (icon_loading_wait_timer_.IsRunning())
    return;

  icon_loading_wait_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kMaxIconLoadingWaitTimeInMs),
      this, &AppListMainView::OnIconLoadingWaitTimer);
}

void AppListMainView::Close() {
  icon_loading_wait_timer_.Stop();
  contents_view_->CancelDrag();
}

void AppListMainView::Prerender() {
  contents_view_->Prerender();
}

void AppListMainView::ModelChanged() {
  pending_icon_loaders_.clear();
  model_ = delegate_->GetModel();
  search_box_view_->ModelChanged();
  delete contents_view_;
  contents_view_ = NULL;
  pagination_model_->SelectPage(0, false /* animate */);
  AddContentsView();
  Layout();
}

void AppListMainView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  contents_view_->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
}

void AppListMainView::PreloadIcons(gfx::NativeView parent) {
  ui::ScaleFactor scale_factor = ui::SCALE_FACTOR_100P;
  if (parent)
    scale_factor = ui::GetScaleFactorForNativeView(parent);

  float scale = ui::GetImageScale(scale_factor);
  // |pagination_model| could have -1 as the initial selected page and
  // assumes first page (i.e. index 0) will be used in this case.
  const int selected_page = std::max(0, pagination_model_->selected_page());

  const int tiles_per_page = kPreferredCols * kPreferredRows;
  const int start_model_index = selected_page * tiles_per_page;
  const int end_model_index = std::min(
      static_cast<int>(model_->item_list()->item_count()),
      start_model_index + tiles_per_page);

  pending_icon_loaders_.clear();
  for (int i = start_model_index; i < end_model_index; ++i) {
    AppListItem* item = model_->item_list()->item_at(i);
    if (item->icon().HasRepresentation(scale))
      continue;

    pending_icon_loaders_.push_back(new IconLoader(this, item, scale));
  }
}

void AppListMainView::OnIconLoadingWaitTimer() {
  GetWidget()->Show();
}

void AppListMainView::OnItemIconLoaded(IconLoader* loader) {
  ScopedVector<IconLoader>::iterator it = std::find(
      pending_icon_loaders_.begin(), pending_icon_loaders_.end(), loader);
  DCHECK(it != pending_icon_loaders_.end());
  pending_icon_loaders_.erase(it);

  if (pending_icon_loaders_.empty() && icon_loading_wait_timer_.IsRunning()) {
    icon_loading_wait_timer_.Stop();
    GetWidget()->Show();
  }
}

void AppListMainView::ActivateApp(AppListItem* item, int event_flags) {
  // TODO(jennyz): Activate the folder via AppListModel notification.
  if (item->GetItemType() == AppListFolderItem::kItemType)
    contents_view_->ShowFolderContent(static_cast<AppListFolderItem*>(item));
  else
    item->Activate(event_flags);
}

void AppListMainView::GetShortcutPathForApp(
    const std::string& app_id,
    const base::Callback<void(const base::FilePath&)>& callback) {
  delegate_->GetShortcutPathForApp(app_id, callback);
}

void AppListMainView::QueryChanged(SearchBoxView* sender) {
  base::string16 query;
  TrimWhitespace(model_->search_box()->text(), TRIM_ALL, &query);
  bool should_show_search = !query.empty();
  contents_view_->ShowSearchResults(should_show_search);

  if (should_show_search)
    delegate_->StartSearch();
  else
    delegate_->StopSearch();
}

void AppListMainView::OpenResult(SearchResult* result,
                                 bool auto_launch,
                                 int event_flags) {
  delegate_->OpenSearchResult(result, auto_launch, event_flags);
}

void AppListMainView::InvokeResultAction(SearchResult* result,
                                         int action_index,
                                         int event_flags) {
  delegate_->InvokeSearchResultAction(result, action_index, event_flags);
}

void AppListMainView::OnResultInstalled(SearchResult* result) {
  // Clears the search to show the apps grid. The last installed app
  // should be highlighted and made visible already.
  search_box_view_->ClearSearch();
}

void AppListMainView::OnResultUninstalled(SearchResult* result) {
  // Resubmit the query via a posted task so that all observers for the
  // uninstall notification are notified.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&AppListMainView::QueryChanged,
                 weak_ptr_factory_.GetWeakPtr(),
                 search_box_view_));
}

}  // namespace app_list
