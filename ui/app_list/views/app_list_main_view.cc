// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_main_view.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/profiler/scoped_tracker.h"
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
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/start_page_view.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace app_list {

namespace {

// The maximum allowed time to wait for icon loading in milliseconds.
const int kMaxIconLoadingWaitTimeInMs = 50;

// Button for the custom page click zone. Receives click events when the user
// clicks on the custom page, and in response, switches to the custom page.
class CustomPageButton : public views::CustomButton,
                         public views::ButtonListener {
 public:
  explicit CustomPageButton(AppListMainView* app_list_main_view);

  // ButtonListener overrides:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  AppListMainView* app_list_main_view_;

  DISALLOW_COPY_AND_ASSIGN(CustomPageButton);
};

CustomPageButton::CustomPageButton(AppListMainView* app_list_main_view)
    : views::CustomButton(this), app_list_main_view_(app_list_main_view) {
}

void CustomPageButton::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  // Switch to the custom page.
  ContentsView* contents_view = app_list_main_view_->contents_view();
  int custom_page_index = contents_view->GetPageIndexForState(
      AppListModel::STATE_CUSTOM_LAUNCHER_PAGE);
  contents_view->SetActivePage(custom_page_index);
}

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

  ~IconLoader() override { item_->RemoveObserver(this); }

 private:
  // AppListItemObserver overrides:
  void ItemIconChanged() override {
    owner_->OnItemIconLoaded(this);
    // Note that IconLoader is released here.
  }

  AppListMainView* owner_;
  AppListItem* item_;

  DISALLOW_COPY_AND_ASSIGN(IconLoader);
};

////////////////////////////////////////////////////////////////////////////////
// AppListMainView:

AppListMainView::AppListMainView(AppListViewDelegate* delegate)
    : delegate_(delegate),
      model_(delegate->GetModel()),
      search_box_view_(nullptr),
      contents_view_(nullptr),
      custom_page_clickzone_(nullptr),
      weak_ptr_factory_(this) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  model_->AddObserver(this);
}

AppListMainView::~AppListMainView() {
  pending_icon_loaders_.clear();
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

  // Starts icon loading early.
  PreloadIcons(parent);
}

void AppListMainView::AddContentsViews() {
  DCHECK(search_box_view_);

  contents_view_ = new ContentsView(this);
  contents_view_->Init(model_);
  AddChildView(contents_view_);

  search_box_view_->set_contents_view(contents_view_);

  contents_view_->SetPaintToLayer(true);
  contents_view_->SetFillsBoundsOpaquely(false);
  contents_view_->layer()->SetMasksToBounds(true);

  delegate_->StartSearch();
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
  if (switches::IsExperimentalAppListEnabled()) {
    contents_view_->SetActivePage(
        contents_view_->GetPageIndexForState(AppListModel::STATE_START));
  }
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
  model_->RemoveObserver(this);
  model_ = delegate_->GetModel();
  model_->AddObserver(this);
  search_box_view_->ModelChanged();
  delete contents_view_;
  contents_view_ = NULL;
  AddContentsViews();
  Layout();
}

views::Widget* AppListMainView::GetCustomPageClickzone() const {
  // During shutdown, the widgets may be deleted, which means
  // |custom_page_clickzone_| will be a dangling pointer. Therefore, always
  // check that the main app list widget (its parent) is still alive before
  // returning the pointer.
  if (!GetWidget())
    return nullptr;

  return custom_page_clickzone_;
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

void AppListMainView::InitWidgets() {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION("431326 AppListMainView::InitWidgets"));

  // The widget that receives click events to transition to the custom page.
  views::Widget::InitParams custom_page_clickzone_params(
      views::Widget::InitParams::TYPE_CONTROL);

  custom_page_clickzone_params.parent = GetWidget()->GetNativeView();
  custom_page_clickzone_params.layer_type = aura::WINDOW_LAYER_NOT_DRAWN;

  gfx::Rect custom_page_bounds = contents_view_->GetCustomPageCollapsedBounds();
  custom_page_bounds.Intersect(contents_view_->bounds());
  custom_page_bounds = contents_view_->ConvertRectToWidget(custom_page_bounds);
  custom_page_clickzone_params.bounds = custom_page_bounds;

  // Create a widget for the custom page click zone. This widget masks click
  // events from the WebContents that rests underneath it. (It has to be a
  // widget, not an ordinary view, so that it can be placed in front of the
  // WebContents.)
  custom_page_clickzone_ = new views::Widget;
  custom_page_clickzone_->Init(custom_page_clickzone_params);
  custom_page_clickzone_->SetContentsView(new CustomPageButton(this));
  // The widget is shown by default. If the ContentsView determines that we do
  // not need a clickzone upon startup, hide it.
  if (!contents_view_->ShouldShowCustomPageClickzone())
    custom_page_clickzone_->Hide();
}

void AppListMainView::OnCustomLauncherPageEnabledStateChanged(bool enabled) {
  // Allow the start page to update |custom_page_clickzone_|.
  if (contents_view_->IsStateActive(AppListModel::STATE_START))
    contents_view_->start_page_view()->UpdateCustomPageClickzoneVisibility();
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

  delegate_->StartSearch();
}

void AppListMainView::BackButtonPressed() {
  contents_view_->Back();
}

void AppListMainView::OnResultInstalled(SearchResult* result) {
  // Clears the search to show the apps grid. The last installed app
  // should be highlighted and made visible already.
  search_box_view_->ClearSearch();
}

}  // namespace app_list
