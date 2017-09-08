// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_main_view.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/app_list/views/app_list_item_view.h"
#include "ui/app_list/views/apps_container_view.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/custom_launcher_page_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/search_result_page_view.h"
#include "ui/app_list/views/start_page_view.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace app_list {

////////////////////////////////////////////////////////////////////////////////
// AppListMainView:

AppListMainView::AppListMainView(AppListViewDelegate* delegate,
                                 AppListView* app_list_view)
    : delegate_(delegate),
      model_(delegate->GetModel()),
      search_box_view_(nullptr),
      contents_view_(nullptr),
      app_list_view_(app_list_view) {
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, gfx::Insets(), 0));
  model_->AddObserver(this);
}

AppListMainView::~AppListMainView() {
  model_->RemoveObserver(this);
}

void AppListMainView::Init(gfx::NativeView parent,
                           int initial_apps_page,
                           SearchBoxView* search_box_view) {
  search_box_view_ = search_box_view;
  AddContentsViews();

  // Switch the apps grid view to the specified page.
  app_list::PaginationModel* pagination_model = GetAppsPaginationModel();
  if (pagination_model->is_valid_page(initial_apps_page))
    pagination_model->SelectPage(initial_apps_page, false);

  OnSearchEngineIsGoogleChanged(model_->search_engine_is_google());
}

void AppListMainView::AddContentsViews() {
  DCHECK(search_box_view_);
  contents_view_ = new ContentsView(this, app_list_view_);
  contents_view_->Init(model_);
  AddChildView(contents_view_);

  search_box_view_->set_contents_view(contents_view_);

  contents_view_->SetPaintToLayer();
  contents_view_->layer()->SetFillsBoundsOpaquely(false);
  contents_view_->layer()->SetMasksToBounds(true);

  // Clear the old query and start search.
  search_box_view_->ClearSearch();
}

void AppListMainView::ShowAppListWhenReady() {
  GetWidget()->Show();
}

void AppListMainView::ResetForShow() {
  contents_view_->SetActiveState(AppListModel::STATE_START);
  contents_view_->apps_container_view()->ResetForShowApps();
  // We clear the search when hiding so when app list appears it is not showing
  // search results.
  search_box_view_->ClearSearch();
}

void AppListMainView::Close() {
  contents_view_->CancelDrag();
}

void AppListMainView::ModelChanged() {
  model_->RemoveObserver(this);
  model_ = delegate_->GetModel();
  model_->AddObserver(this);
  search_box_view_->ModelChanged();
  delete contents_view_;
  contents_view_ = nullptr;
  AddContentsViews();
  Layout();
}

void AppListMainView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  contents_view_->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
}

PaginationModel* AppListMainView::GetAppsPaginationModel() {
  return contents_view_->apps_container_view()
      ->apps_grid_view()
      ->pagination_model();
}

void AppListMainView::NotifySearchBoxVisibilityChanged() {
  // Repaint the AppListView's background which will repaint the background for
  // the search box. This is needed because this view paints to a layer and
  // won't propagate paints upward.
  if (parent())
    parent()->SchedulePaint();
}

bool AppListMainView::ShouldShowCustomLauncherPage() const {
  return contents_view_->custom_page_view() &&
         model_->custom_launcher_page_enabled() &&
         model_->search_engine_is_google();
}

void AppListMainView::UpdateCustomLauncherPageVisibility() {
  views::View* custom_page = contents_view_->custom_page_view();
  if (!custom_page)
    return;

  if (ShouldShowCustomLauncherPage()) {
    // Make the custom page view visible again.
    custom_page->SetVisible(true);
  } else if (contents_view_->IsStateActive(
                 AppListModel::STATE_CUSTOM_LAUNCHER_PAGE)) {
    // Animate to the start page if currently on the custom page view. The view
    // will hide on animation completion.
    contents_view_->SetActiveState(AppListModel::STATE_START);
  } else {
    // Hide the view immediately otherwise.
    custom_page->SetVisible(false);
  }
}

const char* AppListMainView::GetClassName() const {
  return "AppListMainView";
}

void AppListMainView::OnCustomLauncherPageEnabledStateChanged(bool enabled) {
  UpdateCustomLauncherPageVisibility();
}

void AppListMainView::OnSearchEngineIsGoogleChanged(bool is_google) {
  if (contents_view_->custom_page_view())
    UpdateCustomLauncherPageVisibility();

  if (contents_view_->start_page_view()) {
    contents_view_->start_page_view()->instant_container()->SetVisible(
        is_google);
  }
}

void AppListMainView::ActivateApp(AppListItem* item, int event_flags) {
  // TODO(jennyz): Activate the folder via AppListModel notification.
  if (item->GetItemType() == AppListFolderItem::kItemType) {
    contents_view_->ShowFolderContent(static_cast<AppListFolderItem*>(item));
    UMA_HISTOGRAM_ENUMERATION(kAppListFolderOpenedHistogram, kOldFolders,
                              kMaxFolderOpened);
  } else {
    item->Activate(event_flags);
    UMA_HISTOGRAM_BOOLEAN(features::IsFullscreenAppListEnabled()
                              ? kAppListAppLaunchedFullscreen
                              : kAppListAppLaunched,
                          false /*not a suggested app*/);
  }
}

void AppListMainView::CancelDragInActiveFolder() {
  contents_view_->apps_container_view()
      ->app_list_folder_view()
      ->items_grid_view()
      ->EndDrag(true);
}

void AppListMainView::OnResultInstalled(SearchResult* result) {
  // Clears the search to show the apps grid. The last installed app
  // should be highlighted and made visible already.
  search_box_view_->ClearSearch();
}

void AppListMainView::QueryChanged(SearchBoxView* sender) {
  base::string16 query;
  base::TrimWhitespace(model_->search_box()->text(), base::TRIM_ALL, &query);
  bool should_show_search = !query.empty();
  contents_view_->ShowSearchResults(should_show_search);

  delegate_->StartSearch();
}

void AppListMainView::BackButtonPressed() {
  contents_view_->Back();
}

void AppListMainView::SetSearchResultSelection(bool select) {
  if (contents_view_->GetActiveState() == AppListModel::STATE_SEARCH_RESULTS)
    contents_view_->search_results_page_view()->SetSelection(select);
}

}  // namespace app_list
