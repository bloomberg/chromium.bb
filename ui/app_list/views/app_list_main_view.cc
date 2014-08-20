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
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/app_list/views/app_list_item_view.h"
#include "ui/app_list/views/apps_container_view.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/app_list/views/contents_switcher_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace app_list {

namespace {

// Inner padding space in pixels of bubble contents.
const int kInnerPadding = 1;

// The maximum allowed time to wait for icon loading in milliseconds.
const int kMaxIconLoadingWaitTimeInMs = 50;

// A view that holds another view and takes its preferred size. This is used for
// wrapping the search box view so it still gets laid out while hidden. This is
// a separate class so it can notify the main view on search box visibility
// change.
class SearchBoxContainerView : public views::View {
 public:
  SearchBoxContainerView(AppListMainView* host, SearchBoxView* search_box)
      : host_(host), search_box_(search_box) {
    SetLayoutManager(new views::FillLayout());
    AddChildView(search_box);
  }
  virtual ~SearchBoxContainerView() {}

 private:
  // Overridden from views::View:
  virtual void ChildVisibilityChanged(views::View* child) OVERRIDE {
    DCHECK_EQ(search_box_, child);
    host_->NotifySearchBoxVisibilityChanged();
  }

  AppListMainView* host_;
  SearchBoxView* search_box_;

  DISALLOW_COPY_AND_ASSIGN(SearchBoxContainerView);
};

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

  AppListMainView* owner_;
  AppListItem* item_;

  DISALLOW_COPY_AND_ASSIGN(IconLoader);
};

////////////////////////////////////////////////////////////////////////////////
// AppListMainView:

AppListMainView::AppListMainView(AppListViewDelegate* delegate,
                                 int initial_apps_page,
                                 gfx::NativeView parent)
    : delegate_(delegate),
      model_(delegate->GetModel()),
      search_box_view_(NULL),
      contents_view_(NULL),
      contents_switcher_view_(NULL),
      weak_ptr_factory_(this) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
                                        kInnerPadding,
                                        kInnerPadding,
                                        kInnerPadding));

  search_box_view_ = new SearchBoxView(this, delegate);
  AddChildView(new SearchBoxContainerView(this, search_box_view_));
  AddContentsViews();

  // Switch the apps grid view to the specified page.
  app_list::PaginationModel* pagination_model = GetAppsPaginationModel();
  if (pagination_model->is_valid_page(initial_apps_page))
    pagination_model->SelectPage(initial_apps_page, false);

  // Starts icon loading early.
  PreloadIcons(parent);
}

void AppListMainView::AddContentsViews() {
  contents_view_ = new ContentsView(this);
  if (app_list::switches::IsExperimentalAppListEnabled()) {
    contents_switcher_view_ = new ContentsSwitcherView(contents_view_);
    contents_view_->SetContentsSwitcherView(contents_switcher_view_);
  }
  contents_view_->InitNamedPages(model_, delegate_);
  AddChildView(contents_view_);
  if (contents_switcher_view_)
    AddChildView(contents_switcher_view_);

  search_box_view_->set_contents_view(contents_view_);

  contents_view_->SetPaintToLayer(true);
  contents_view_->SetFillsBoundsOpaquely(false);
  contents_view_->layer()->SetMasksToBounds(true);

  delegate_->StartSearch();
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

void AppListMainView::ResetForShow() {
  contents_view_->apps_container_view()->ResetForShowApps();
  // We clear the search when hiding so when app list appears it is not showing
  // search results.
  search_box_view_->ClearSearch();
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
  if (contents_switcher_view_) {
    delete contents_switcher_view_;
    contents_switcher_view_ = NULL;
  }
  AddContentsViews();
  Layout();
}

void AppListMainView::UpdateSearchBoxVisibility() {
  bool visible =
      !contents_view_->IsNamedPageActive(ContentsView::NAMED_PAGE_START) ||
      contents_view_->IsShowingSearchResults();
  search_box_view_->SetVisible(visible);
  if (visible && GetWidget() && GetWidget()->IsVisible())
    search_box_view_->search_box()->RequestFocus();
}

void AppListMainView::OnStartPageSearchTextfieldChanged(
    const base::string16& new_contents) {
  search_box_view_->SetVisible(true);
  search_box_view_->search_box()->SetText(new_contents);
  search_box_view_->search_box()->RequestFocus();
}

void AppListMainView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  contents_view_->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
}

bool AppListMainView::ShouldCenterWindow() const {
  return delegate_->ShouldCenterWindow();
}

PaginationModel* AppListMainView::GetAppsPaginationModel() {
  return contents_view_->apps_container_view()
      ->apps_grid_view()
      ->pagination_model();
}

void AppListMainView::PreloadIcons(gfx::NativeView parent) {
  float scale_factor = 1.0f;
  if (parent)
    scale_factor = ui::GetScaleFactorForNativeView(parent);

  // The PaginationModel could have -1 as the initial selected page and
  // assumes first page (i.e. index 0) will be used in this case.
  const int selected_page =
      std::max(0, GetAppsPaginationModel()->selected_page());

  const AppsGridView* const apps_grid_view =
      contents_view_->apps_container_view()->apps_grid_view();
  const int tiles_per_page =
      apps_grid_view->cols() * apps_grid_view->rows_per_page();

  const int start_model_index = selected_page * tiles_per_page;
  const int end_model_index =
      std::min(static_cast<int>(model_->top_level_item_list()->item_count()),
               start_model_index + tiles_per_page);

  pending_icon_loaders_.clear();
  for (int i = start_model_index; i < end_model_index; ++i) {
    AppListItem* item = model_->top_level_item_list()->item_at(i);
    if (item->icon().HasRepresentation(scale_factor))
      continue;

    pending_icon_loaders_.push_back(new IconLoader(this, item, scale_factor));
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

void AppListMainView::NotifySearchBoxVisibilityChanged() {
  // Repaint the AppListView's background which will repaint the background for
  // the search box. This is needed because this view paints to a layer and
  // won't propagate paints upward.
  if (parent())
    parent()->SchedulePaint();
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

void AppListMainView::CancelDragInActiveFolder() {
  contents_view_->apps_container_view()
      ->app_list_folder_view()
      ->items_grid_view()
      ->EndDrag(true);
}

void AppListMainView::QueryChanged(SearchBoxView* sender) {
  base::string16 query;
  base::TrimWhitespace(model_->search_box()->text(), base::TRIM_ALL, &query);
  bool should_show_search = !query.empty();
  contents_view_->ShowSearchResults(should_show_search);
  UpdateSearchBoxVisibility();

  delegate_->StartSearch();
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
