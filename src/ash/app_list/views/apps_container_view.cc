// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/apps_container_view.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/views/app_list_a11y_announcer.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/continue_section_view.h"
#include "ash/app_list/views/folder_background_view.h"
#include "ash/app_list/views/page_switcher.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/suggestion_chip_container_view.h"
#include "ash/constants/ash_features.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_model_delegate.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/search_box/search_box_constants.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/cxx17_backports.h"
#include "base/metrics/histogram_macros.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

// The number of rows for portrait mode with mode productivity launcher
// enabled.
constexpr int kPreferredGridRowsInPortraitProductivityLauncher = 5;

// The number of columns for portrait mode with mode productivity launcher
// enabled.
constexpr int kPreferredGridColumnsInPortraitProductivityLauncher = 5;

// The long apps grid dimension when productivity launcher is not enabled:
// * number of columns in landscape mode
// * number of rows in portrait mode
constexpr int kPreferredGridColumns = 5;

// The short apps grid dimension when productivity launcher is not enabled:
// * number of rows in landscape mode
// * number of columns in portrait mode
constexpr int kPreferredGridRows = 4;

// The range of app list transition progress in which the suggestion chips'
// opacity changes from 0 to 1.
constexpr float kSuggestionChipOpacityStartProgress = 0.66;
constexpr float kSuggestionChipOpacityEndProgress = 1;

// Range of the height of centerline above screen bottom that all apps should
// change opacity. NOTE: this is used to change page switcher's opacity as
// well.
constexpr float kAppsOpacityChangeStart = 8.0f;
constexpr float kAppsOpacityChangeEnd = 144.0f;

// The app list transition progress value for fullscreen state.
constexpr float kAppListFullscreenProgressValue = 2.0;

// The amount by which the apps container UI should be offset downwards when
// shown on non apps page UI.
constexpr int kNonAppsStateVerticalOffset = 24;

// The opacity the apps container UI should have when shown on non apps page UI.
constexpr float kNonAppsStateOpacity = 0.1;

// The ratio of allowed bounds for apps grid view to its maximum margin.
constexpr int kAppsGridMarginRatio = 16;
constexpr int kAppsGridMarginRatioForSmallWidth = 12;

// The margins within the apps container for app list folder view.
constexpr int kFolderMargin = 8;

// The suggestion chip container height.
constexpr int kSuggestionChipContainerHeight = 32;

// The suggestion chip container top margin.
constexpr int kSuggestionChipContainerTopMargin = 16;

// The horizontal margin between the apps grid view and the page switcher.
constexpr int kGridToPageSwitcherMargin = 8;

// Minimal horizontal distance from the page switcher to apps container bounds.
constexpr int kPageSwitcherEndMargin = 16;

// The apps grid view's fadeout zone height, that contains a fadeout mask, and
// which is used as a margin for the `AppsGridView` contents.
constexpr int kGridFadeoutZoneHeight = 24;

// The space between sort ui controls (including sort button and redo button).
constexpr int kSortUiControlSpacing = 10;

// The preferred size of sort ui controls (like sort button and redo button).
constexpr int kSortUiControlPreferredSize = 20;

// The number of columns available for the ContinueSectionView.
constexpr int kContinueColumnCount = 4;

// The vertical spacing above and below the separator.
constexpr int kSeparatorVerticalInset = 16;

// The width of the separator.
constexpr int kSeparatorWidth = 240;

// SortUiControl ---------------------------------------------------------------

class SortUiControl : public views::ImageButton {
 public:
  SortUiControl(AppListViewDelegate* delegate,
                views::Button::PressedCallback pressed_callback)
      : views::ImageButton(pressed_callback), delegate_(delegate) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
    SetPreferredSize(
        gfx::Size(kSortUiControlPreferredSize, kSortUiControlPreferredSize));

    // This view is used behind the feature flag and is immature. Therefore
    // ignore it in a11y for now.
    GetViewAccessibility().OverrideIsIgnored(true);
  }
  SortUiControl(const SortUiControl&) = delete;
  SortUiControl& operator=(const SortUiControl&) = delete;
  ~SortUiControl() override = default;

  // views::View:
  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    StyleUtil::ConfigureInkDropAttributes(
        this, StyleUtil::kBaseColor | StyleUtil::kInkDropOpacity |
                  StyleUtil::kHighlightOpacity);
    views::InstallFixedSizeCircleHighlightPathGenerator(
        this, kSortUiControlPreferredSize / 2);
    views::InkDrop::UseInkDropForFloodFillRipple(views::InkDrop::Get(this),
                                                 /*highlight_on_hover=*/true,
                                                 /*highlight_on_focus=*/true);
  }

 protected:
  AppListViewDelegate* const delegate_;
};

// RedoButton ------------------------------------------------------------------

// A button for reverting the temporary sort order if any.
// TODO(https://crbug.com/1263999): remove `RedoButton` when the app list sort
// is enabled as default.
class RedoButton : public SortUiControl {
 public:
  METADATA_HEADER(RedoButton);

  explicit RedoButton(AppListViewDelegate* delegate)
      : SortUiControl(delegate,
                      base::BindRepeating(&RedoButton::RevertAppListSort,
                                          base::Unretained(this))) {}
  RedoButton(const RedoButton&) = delete;
  RedoButton& operator=(const RedoButton&) = delete;
  ~RedoButton() override = default;

 private:
  // views::View:
  void OnThemeChanged() override {
    SortUiControl::OnThemeChanged();
    const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kButtonIconColor);
    SetImage(views::Button::STATE_NORMAL,
             gfx::CreateVectorIcon(kSmallCloseButtonIcon,
                                   GetPreferredSize().width(), icon_color));
  }

  void RevertAppListSort() {
    views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(
        views::InkDropState::ACTION_TRIGGERED);
    AppListModelProvider::Get()
        ->model()
        ->delegate()
        ->RequestAppListSortRevert();
  }
};

BEGIN_METADATA(RedoButton, views::View)
END_METADATA

// SortUiControlContainer ------------------------------------------------------

class SortUiControlContainer : public views::View {
 public:
  METADATA_HEADER(SortUiControlContainer);

  explicit SortUiControlContainer(AppListViewDelegate* delegate) {
    // The layer is required in animation.
    SetPaintToLayer(ui::LayerType::LAYER_NOT_DRAWN);

    // Configure the layout.
    auto box_layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        /*inside_border_insets=*/gfx::Insets(),
        /*between_child_spacing=*/kSortUiControlSpacing);
    box_layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    box_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    SetLayoutManager(std::move(box_layout));

    // Add children.
    AddChildView(std::make_unique<RedoButton>(delegate));

    GetViewAccessibility().OverrideIsIgnored(true);
  }
  SortUiControlContainer(const SortUiControlContainer&) = delete;
  SortUiControlContainer& operator=(const SortUiControlContainer&) = delete;
  ~SortUiControlContainer() override = default;
};

BEGIN_METADATA(SortUiControlContainer, views::View)
END_METADATA

}  // namespace

// A view that contains continue section, recent apps and a separator view,
// which is shown when any of other views is shown.
// The view is intended to be a wrapper around suggested content views that
// makes applying identical transforms to suggested content views easier.
class AppsContainerView::ContinueContainer : public views::View {
 public:
  ContinueContainer(AppsContainerView* apps_container,
                    AppListViewDelegate* view_delegate) {
    SetPaintToLayer(ui::LAYER_NOT_DRAWN);
    SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kVertical);

    continue_section_ = AddChildView(std::make_unique<ContinueSectionView>(
        view_delegate, kContinueColumnCount, /*tablet_mode=*/true));
    continue_section_->SetPaintToLayer();
    continue_section_->layer()->SetFillsBoundsOpaquely(false);
    continue_section_->UpdateSuggestionTasks();

    recent_apps_ = AddChildView(
        std::make_unique<RecentAppsView>(apps_container, view_delegate));
    recent_apps_->SetPaintToLayer();
    recent_apps_->layer()->SetFillsBoundsOpaquely(false);

    separator_ = AddChildView(std::make_unique<views::Separator>());
    separator_->SetColor(ColorProvider::Get()->GetContentLayerColor(
        ColorProvider::ContentLayerType::kSeparatorColor));
    separator_->SetBorder(
        views::CreateEmptyBorder(gfx::Insets(kSeparatorVerticalInset, 0)));
    separator_->SetPreferredSize(
        gfx::Size(kSeparatorWidth,
                  kSeparatorVerticalInset * 2 + views::Separator::kThickness));
    separator_->SetPaintToLayer();
    separator_->layer()->SetFillsBoundsOpaquely(false);
    separator_->SetProperty(views::kCrossAxisAlignmentKey,
                            views::LayoutAlignment::kCenter);

    UpdateSeparatorVisibility();
  }

  // views::View:
  void ChildVisibilityChanged(views::View* child) override {
    if (child == recent_apps_ || child == continue_section_)
      UpdateSeparatorVisibility();
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    separator_->SetColor(ColorProvider::Get()->GetContentLayerColor(
        ColorProvider::ContentLayerType::kSeparatorColor));
  }

  ContinueSectionView* continue_section() { return continue_section_; }
  RecentAppsView* recent_apps() { return recent_apps_; }
  views::View* separator() { return separator_; }

 private:
  void UpdateSeparatorVisibility() {
    if (!separator_ || !recent_apps_ || !continue_section_)
      return;
    separator_->SetVisible(recent_apps_->GetVisible() ||
                           continue_section_->GetVisible());
  }

  ContinueSectionView* continue_section_ = nullptr;
  RecentAppsView* recent_apps_ = nullptr;
  views::Separator* separator_ = nullptr;
};

AppsContainerView::AppsContainerView(ContentsView* contents_view)
    : contents_view_(contents_view) {
  AppListModelProvider::Get()->AddObserver(this);

  SetPaintToLayer(ui::LAYER_NOT_DRAWN);

  scrollable_container_ = AddChildView(std::make_unique<views::View>());
  scrollable_container_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  AppListViewDelegate* view_delegate =
      contents_view_->GetAppListMainView()->view_delegate();

  if (features::IsProductivityLauncherEnabled()) {
    // The bounds of the |scrollable_container_| will visually clip the
    // |continue_container_| layer during page changes.
    scrollable_container_->layer()->SetMasksToBounds(true);
    continue_container_ = scrollable_container_->AddChildView(
        std::make_unique<ContinueContainer>(this, view_delegate));
  } else {
    // Add child view at index 0 so focus traversal goes to suggestion chips
    // before the views in the scrollable_container.
    suggestion_chip_container_view_ = AddChildViewAt(
        std::make_unique<SuggestionChipContainerView>(contents_view), 0);
  }

  AppListA11yAnnouncer* a11y_announcer =
      contents_view->app_list_view()->a11y_announcer();
  // Add `apps_grid_view_` at index 0 to put it at the back and ensure other
  // views in the `scrollable_container` get events first, since the grid
  // overlaps in bounds with these other views.
  apps_grid_view_ = scrollable_container_->AddChildViewAt(
      std::make_unique<PagedAppsGridView>(contents_view, a11y_announcer,
                                          /*folder_delegate=*/nullptr,
                                          /*folder_controller=*/this,
                                          /*container_delegate=*/this),
      0);
  apps_grid_view_->Init();

  if (features::IsProductivityLauncherEnabled())
    apps_grid_view_->pagination_model()->AddObserver(this);

  // Page switcher should be initialized after AppsGridView.
  auto page_switcher = std::make_unique<PageSwitcher>(
      apps_grid_view_->pagination_model(), true /* vertical */,
      contents_view->app_list_view()->is_tablet_mode());
  page_switcher_ = AddChildView(std::move(page_switcher));

  auto app_list_folder_view = std::make_unique<AppListFolderView>(
      this, apps_grid_view_, contents_view_, a11y_announcer, view_delegate);
  folder_background_view_ = AddChildView(
      std::make_unique<FolderBackgroundView>(app_list_folder_view.get()));

  app_list_folder_view_ = AddChildView(std::move(app_list_folder_view));
  // The folder view is initially hidden.
  app_list_folder_view_->SetVisible(false);

  if (features::IsLauncherAppSortEnabled()) {
    sort_button_container_ =
        AddChildView(std::make_unique<SortUiControlContainer>(view_delegate));
  }

  // NOTE: At this point, the apps grid folder and recent apps grids are not
  // fully initialized - they require an `app_list_config_` instance (because
  // they contain AppListItemView), which in turn requires widget, and the
  // view's contents bounds to be correctly calculated. The initialization
  // will be completed in `OnBoundsChanged()` when the apps container bounds are
  // first set.
}

AppsContainerView::~AppsContainerView() {
  AppListModelProvider::Get()->RemoveObserver(this);

  if (features::IsProductivityLauncherEnabled())
    apps_grid_view_->pagination_model()->RemoveObserver(this);

  // Make sure |page_switcher_| is deleted before |apps_grid_view_| because
  // |page_switcher_| uses the PaginationModel owned by |apps_grid_view_|.
  delete page_switcher_;

  // App list folder view, if shown, may reference/observe a root apps grid view
  // item (associated with the item for which the folder is shown). Delete
  // `app_list_folder_view_` explicitly to ensure it's deleted before
  // `apps_grid_view_`.
  delete app_list_folder_view_;
}

void AppsContainerView::UpdateTopLevelGridDimensions() {
  const GridLayout grid_layout = CalculateGridLayout();
  if (features::IsProductivityLauncherEnabled()) {
    apps_grid_view_->SetMaxColumnsAndRows(
        /*max_columns=*/grid_layout.columns,
        /*max_rows_on_first_page=*/grid_layout.first_page_rows,
        /*max_rows=*/grid_layout.rows);
  } else {
    apps_grid_view_->SetMaxColumnsAndRows(
        /*max_columns=*/grid_layout.columns,
        /*max_rows_on_first_page=*/grid_layout.first_page_rows,
        /*max_rows=*/grid_layout.rows);
  }
}

gfx::Rect AppsContainerView::CalculateAvailableBoundsForAppsGrid(
    const gfx::Rect& contents_bounds) const {
  gfx::Rect available_bounds = contents_bounds;
  // Reserve horizontal margins to accommodate page switcher.
  available_bounds.Inset(GetMinHorizontalMarginForAppsGrid(), 0);
  // Reserve vertical space for search box and suggestion chips.
  available_bounds.Inset(
      0,
      GetMinTopMarginForAppsGrid(
          contents_view_->GetSearchBoxSize(AppListState::kStateApps)),
      0, 0);
  // Subtracts apps grid view insets from space available for apps grid.
  available_bounds.Inset(0, kGridFadeoutZoneHeight);

  return available_bounds;
}

void AppsContainerView::UpdateAppListConfig(const gfx::Rect& contents_bounds) {
  // For productivity launcher, the rows for this grid layout will be ignored
  // during creation of a new config.
  GridLayout grid_layout = CalculateGridLayout();

  const gfx::Rect available_bounds =
      CalculateAvailableBoundsForAppsGrid(contents_bounds);

  std::unique_ptr<AppListConfig> new_config =
      AppListConfigProvider::Get().CreateForFullscreenAppList(
          display::Screen::GetScreen()
              ->GetDisplayNearestView(GetWidget()->GetNativeView())
              .work_area()
              .size(),
          grid_layout.rows, grid_layout.columns, available_bounds.size(),
          app_list_config_.get());

  // `CreateForFullscreenAppList()` will create a new config only if it differs
  // from the current `app_list_config_`. Nothing to do if the old
  // `AppListConfig` can be used for the updated apps container bounds.
  if (!new_config)
    return;

  // Keep old config around until child views have been updated to use the new
  // config.
  auto old_config = std::move(app_list_config_);
  app_list_config_ = std::move(new_config);

  // Invalidate the cached container margins - app list config change generally
  // changes preferred apps grid margins, which can influence the container
  // margins.
  cached_container_margins_ = CachedContainerMargins();

  apps_grid_view()->UpdateAppListConfig(app_list_config_.get());
  app_list_folder_view()->UpdateAppListConfig(app_list_config_.get());
  if (GetRecentApps())
    GetRecentApps()->UpdateAppListConfig(app_list_config_.get());
}

void AppsContainerView::OnActiveAppListModelsChanged(
    AppListModel* model,
    SearchModel* search_model) {
  // Nothing to do if the apps grid views have not yet been initialized.
  if (!app_list_config_)
    return;

  UpdateForActiveAppListModel();
}

void AppsContainerView::ShowFolderForItemView(
    AppListItemView* folder_item_view) {
  // Prevent new animations from starting if there are currently animations
  // pending. This fixes crbug.com/357099.
  if (app_list_folder_view_->IsAnimationRunning())
    return;

  DCHECK(folder_item_view->is_folder());

  UMA_HISTOGRAM_ENUMERATION("Apps.AppListFolderOpened",
                            kFullscreenAppListFolders, kMaxFolderOpened);

  app_list_folder_view_->ConfigureForFolderItemView(folder_item_view);
  SetShowState(SHOW_ACTIVE_FOLDER, false);

  // If there is no selected view in the root grid when a folder is opened,
  // silently focus the first item in the folder to avoid showing the selection
  // highlight or announcing to A11y, but still ensuring the arrow keys navigate
  // from the first item.
  const bool silently = !apps_grid_view()->has_selected_view();
  app_list_folder_view_->FocusFirstItem(silently);

  // Disable all the items behind the folder so that they will not be reached
  // during focus traversal.
  DisableFocusForShowingActiveFolder(true);
}

void AppsContainerView::ShowApps(AppListItemView* folder_item_view,
                                 bool select_folder) {
  DVLOG(1) << __FUNCTION__;
  if (app_list_folder_view_->IsAnimationRunning())
    return;

  const bool animate = !!folder_item_view;
  SetShowState(SHOW_APPS, animate);
  DisableFocusForShowingActiveFolder(false);
  if (folder_item_view) {
    // Focus `folder_item_view` but only show the selection highlight if there
    // was already one showing.
    if (select_folder)
      folder_item_view->RequestFocus();
    else
      folder_item_view->SilentlyRequestFocus();
  }
}

void AppsContainerView::ResetForShowApps() {
  DVLOG(1) << __FUNCTION__;
  UpdateSuggestionChips();
  UpdateRecentApps();
  SetShowState(SHOW_APPS, false);
  DisableFocusForShowingActiveFolder(false);
}

void AppsContainerView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  apps_grid_view()->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
  app_list_folder_view()->items_grid_view()->SetDragAndDropHostOfCurrentAppList(
      drag_and_drop_host);
}

void AppsContainerView::ReparentFolderItemTransit(
    AppListFolderItem* folder_item) {
  if (app_list_folder_view_->IsAnimationRunning())
    return;
  SetShowState(SHOW_ITEM_REPARENT, false);
  DisableFocusForShowingActiveFolder(false);
}

bool AppsContainerView::IsInFolderView() const {
  return show_state_ == SHOW_ACTIVE_FOLDER;
}

void AppsContainerView::ReparentDragEnded() {
  DVLOG(1) << __FUNCTION__;
  // The container will be showing apps if the folder was deleted mid-drag.
  if (show_state_ == SHOW_APPS)
    return;
  DCHECK_EQ(SHOW_ITEM_REPARENT, show_state_);
  show_state_ = AppsContainerView::SHOW_APPS;
}

// PaginationModelObserver:
void AppsContainerView::SelectedPageChanged(int old_selected,
                                            int new_selected) {
  // |continue_container_| is hidden above the grid when not on the first page.
  gfx::Transform transform;
  gfx::Vector2dF translate;
  translate.set_y(-scrollable_container_->bounds().height() * new_selected);
  transform.Translate(translate);
  continue_container_->layer()->SetTransform(transform);
}

void AppsContainerView::TransitionChanged() {
  auto* pagination_model = apps_grid_view_->pagination_model();
  const PaginationModel::Transition& transition =
      pagination_model->transition();
  if (!pagination_model->is_valid_page(transition.target_page))
    return;

  // Because |continue_container_| only shows on the first page, only update its
  // transform if its page is involved in the transition. Otherwise, there is
  // no need to transform the |continue_container_| because it will remain
  // hidden throughout the transition.
  if (transition.target_page == 0 || pagination_model->selected_page() == 0) {
    const int page_height = scrollable_container_->bounds().height();
    gfx::Vector2dF translate;

    if (transition.target_page == 0) {
      // Scroll the continue section down from above.
      translate.set_y(-page_height + page_height * transition.progress);
    } else {
      // Scroll the continue section upwards
      translate.set_y(-page_height * transition.progress);
    }
    gfx::Transform transform;
    transform.Translate(translate);
    continue_container_->layer()->SetTransform(transform);
  }
}

// PagedAppsGridView::ContainerDelegate:
bool AppsContainerView::IsPointWithinPageFlipBuffer(
    const gfx::Point& point_in_apps_grid) const {
  // The page flip buffer is the work area bounds excluding shelf bounds, which
  // is the same as AppsContainerView's bounds.
  gfx::Point point = point_in_apps_grid;
  ConvertPointToTarget(apps_grid_view_, this, &point);
  return this->GetContentsBounds().Contains(point);
}

bool AppsContainerView::IsPointWithinBottomDragBuffer(
    const gfx::Point& point,
    int page_flip_zone_size) const {
  // The bottom drag buffer is between the bottom of apps grid and top of shelf.
  gfx::Point point_in_parent = point;
  ConvertPointToTarget(apps_grid_view_, this, &point_in_parent);
  gfx::Rect parent_rect = this->GetContentsBounds();
  const int kBottomDragBufferMax = parent_rect.bottom();
  const int kBottomDragBufferMin = scrollable_container_->bounds().bottom() -
                                   apps_grid_view_->GetInsets().bottom() -
                                   page_flip_zone_size;
  // TODO(crbug.com/1234064): In ProductivityLauncher, with a variable row size,
  // the size of the bottom drag buffer can visually change. Figure out how we
  // want to handle this and update this code to reflect that.
  return point_in_parent.y() > kBottomDragBufferMin &&
         point_in_parent.y() < kBottomDragBufferMax;
}

// RecentAppsView::Delegate:
void AppsContainerView::MoveFocusUpFromRecents() {
  DCHECK(!GetRecentApps()->children().empty());
  views::View* first_recent = GetRecentApps()->children()[0];
  DCHECK(views::IsViewClass<AppListItemView>(first_recent));
  // Find the view one step in reverse from the first recent app.
  views::View* previous_view = GetFocusManager()->GetNextFocusableView(
      first_recent, GetWidget(), /*reverse=*/true, /*dont_loop=*/false);
  DCHECK(previous_view);
  previous_view->RequestFocus();
}

void AppsContainerView::MoveFocusDownFromRecents(int column) {
  int top_level_item_count = apps_grid_view_->view_model()->view_size();
  if (top_level_item_count <= 0)
    return;
  // Attempt to focus the item at `column` in the first row, or the last item if
  // there aren't enough items. This could happen if the user's apps are in a
  // small number of folders.
  int index = std::min(column, top_level_item_count - 1);
  AppListItemView* item = apps_grid_view_->GetItemViewAt(index);
  DCHECK(item);
  item->RequestFocus();
}

ContinueSectionView* AppsContainerView::GetContinueSection() {
  if (!continue_container_)
    return nullptr;
  return continue_container_->continue_section();
}

RecentAppsView* AppsContainerView::GetRecentApps() {
  if (!continue_container_)
    return nullptr;
  return continue_container_->recent_apps();
}

views::View* AppsContainerView::GetSeparatorView() {
  if (!continue_container_)
    return nullptr;
  return continue_container_->separator();
}

void AppsContainerView::UpdateControlVisibility(AppListViewState app_list_state,
                                                bool is_in_drag) {
  if (app_list_state == AppListViewState::kClosed)
    return;

  SetCanProcessEventsWithinSubtree(
      app_list_state == AppListViewState::kFullscreenAllApps ||
      app_list_state == AppListViewState::kPeeking);

  apps_grid_view_->UpdateControlVisibility(app_list_state, is_in_drag);
  page_switcher_->SetVisible(
      is_in_drag || app_list_state == AppListViewState::kFullscreenAllApps ||
      app_list_state == AppListViewState::kFullscreenSearch);

  // Ignore button press during dragging to avoid app list item views' opacity
  // being set to wrong value.
  page_switcher_->set_ignore_button_press(is_in_drag);

  if (suggestion_chip_container_view_) {
    suggestion_chip_container_view_->SetVisible(
        app_list_state == AppListViewState::kFullscreenAllApps ||
        app_list_state == AppListViewState::kPeeking || is_in_drag);
  }
}

void AppsContainerView::AnimateOpacity(float current_progress,
                                       AppListViewState target_view_state,
                                       const OpacityAnimator& animator) {
  if (suggestion_chip_container_view_) {
    const bool target_suggestion_chip_visibility =
        target_view_state == AppListViewState::kFullscreenAllApps ||
        target_view_state == AppListViewState::kPeeking;
    animator.Run(suggestion_chip_container_view_,
                 target_suggestion_chip_visibility);
  }

  if (!apps_grid_view_->layer()->GetAnimator()->IsAnimatingProperty(
          ui::LayerAnimationElement::OPACITY)) {
    apps_grid_view_->UpdateOpacity(true /*restore_opacity*/,
                                   kAppsOpacityChangeStart,
                                   kAppsOpacityChangeEnd);
    apps_grid_view_->layer()->SetOpacity(current_progress > 1.0f ? 1.0f : 0.0f);
  }

  const bool target_grid_visibility =
      target_view_state == AppListViewState::kFullscreenAllApps ||
      target_view_state == AppListViewState::kFullscreenSearch;
  animator.Run(apps_grid_view_, target_grid_visibility);
  animator.Run(page_switcher_, target_grid_visibility);
}

void AppsContainerView::AnimateYPosition(AppListViewState target_view_state,
                                         const TransformAnimator& animator,
                                         float default_offset) {
  // Apps container position is calculated for app list progress relative to
  // peeking state, which may not match the progress value used to calculate
  // |default_offset| - when showing search results page, the transform offset
  // is calculated using progress relative to AppListViewState::kHalf.
  const float progress =
      contents_view_->app_list_view()->GetAppListTransitionProgress(
          AppListView::kProgressFlagNone |
          AppListView::kProgressFlagWithTransform);
  const int current_suggestion_chip_y = GetExpectedSuggestionChipY(progress);
  const int target_suggestion_chip_y = GetExpectedSuggestionChipY(
      AppListView::GetTransitionProgressForState(target_view_state));
  const int offset = current_suggestion_chip_y - target_suggestion_chip_y;

  if (suggestion_chip_container_view_) {
    suggestion_chip_container_view_->SetY(target_suggestion_chip_y);
    animator.Run(offset, suggestion_chip_container_view_->layer());
  }

  scrollable_container_->SetY(target_suggestion_chip_y + chip_grid_y_distance_);
  animator.Run(offset, scrollable_container_->layer());
  page_switcher_->SetY(target_suggestion_chip_y + chip_grid_y_distance_);
  animator.Run(offset, page_switcher_->layer());

  if (features::IsLauncherAppSortEnabled()) {
    sort_button_container_->SetY(target_suggestion_chip_y);
    animator.Run(offset, sort_button_container_->layer());
  }
}

void AppsContainerView::OnTabletModeChanged(bool started) {
  if (suggestion_chip_container_view_)
    suggestion_chip_container_view_->OnTabletModeChanged(started);
  apps_grid_view_->OnTabletModeChanged(started);
  app_list_folder_view_->OnTabletModeChanged(started);
  page_switcher_->set_is_tablet_mode(started);
}

void AppsContainerView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  // Layout suggestion chips.
  gfx::Rect chip_container_rect = rect;
  chip_container_rect.set_y(GetExpectedSuggestionChipY(
      contents_view_->app_list_view()->GetAppListTransitionProgress(
          AppListView::kProgressFlagNone)));

  if (suggestion_chip_container_view_) {
    chip_container_rect.set_height(kSuggestionChipContainerHeight);
    chip_container_rect.Inset(GetIdealHorizontalMargin(), 0);
    suggestion_chip_container_view_->SetBoundsRect(chip_container_rect);
  } else {
    chip_container_rect.set_height(0);
  }

  // Set bounding box for the folder view - the folder may overlap with
  // suggestion chips, but not the search box.
  gfx::Rect folder_bounding_box = rect;
  folder_bounding_box.Inset(kFolderMargin, chip_container_rect.y(),
                            kFolderMargin, kFolderMargin);
  app_list_folder_view_->SetBoundingBox(folder_bounding_box);

  // Leave the same available bounds for the apps grid view in both
  // fullscreen and peeking state to avoid resizing the view during
  // animation and dragging, which is an expensive operation.
  rect.set_y(chip_container_rect.bottom());
  rect.set_height(rect.height() -
                  GetExpectedSuggestionChipY(kAppListFullscreenProgressValue) -
                  chip_container_rect.height());

  // Layout apps grid.
  const gfx::Insets grid_insets = apps_grid_view_->GetInsets();
  const gfx::Insets margins = CalculateMarginsForAvailableBounds(
      GetContentsBounds(),
      contents_view_->GetSearchBoxSize(AppListState::kStateApps));
  gfx::Rect grid_rect = rect;
  grid_rect.Inset(margins.left(), kGridFadeoutZoneHeight - grid_insets.top(),
                  margins.right(), margins.bottom());
  // The grid rect insets are added to calculated margins. Given that the
  // grid bounds rect should include insets, they have to be removed from
  // added margins.
  grid_rect.Inset(-grid_insets.left(), 0, -grid_insets.right(),
                  -grid_insets.bottom());
  scrollable_container_->SetBoundsRect(grid_rect);
  if (features::IsProductivityLauncherEnabled()) {
    continue_container_->SetBoundsRect(
        gfx::Rect(0, 0, grid_rect.width(),
                  continue_container_->GetPreferredSize().height()));
    // Setting this offset prevents the app items in the grid from overlapping
    // with the continue section.
    apps_grid_view_->set_first_page_offset(
        continue_container_->bounds().height());
  }

  // Make sure that UpdateTopLevelGridDimensions() happens after setting the
  // apps grid's first page offset, because it can change the number of rows
  // shown in the grid.
  UpdateTopLevelGridDimensions();

  apps_grid_view_->SetBoundsRect(
      gfx::Rect(0, 0, grid_rect.width(), grid_rect.height()));

  // Record the distance of y position between suggestion chip container
  // and apps grid view to avoid duplicate calculation of apps grid view's
  // y position during dragging.
  chip_grid_y_distance_ = scrollable_container_->y() - chip_container_rect.y();

  // Layout page switcher.
  const int page_switcher_width = page_switcher_->GetPreferredSize().width();
  const gfx::Rect page_switcher_bounds(
      grid_rect.right() + kGridToPageSwitcherMargin, grid_rect.y(),
      page_switcher_width, grid_rect.height());
  page_switcher_->SetBoundsRect(page_switcher_bounds);

  if (features::IsLauncherAppSortEnabled()) {
    // Align `sort_button_container_` with the bottom of the
    // `suggestion_chip_container_view_` horizontally; align
    // `sort_button_container_` with `page_switcher_bounds` vertically on the
    // right edge.
    const int sort_button_container_width =
        sort_button_container_->GetPreferredSize().width();
    gfx::Rect sort_button_container_rect(
        page_switcher_bounds.right() - sort_button_container_width,
        chip_container_rect.bottom(), sort_button_container_width, 20);
    sort_button_container_->SetBoundsRect(sort_button_container_rect);
  }

  switch (show_state_) {
    case SHOW_APPS:
      break;
    case SHOW_ACTIVE_FOLDER: {
      app_list_folder_view_->UpdatePreferredBounds();
      folder_background_view_->SetBoundsRect(rect);
      app_list_folder_view_->SetBoundsRect(
          app_list_folder_view_->preferred_bounds());
      break;
    }
    case SHOW_ITEM_REPARENT:
    case SHOW_NONE:
      break;
  }
}

bool AppsContainerView::OnKeyPressed(const ui::KeyEvent& event) {
  if (show_state_ == SHOW_APPS)
    return apps_grid_view_->OnKeyPressed(event);
  else
    return app_list_folder_view_->OnKeyPressed(event);
}

const char* AppsContainerView::GetClassName() const {
  return "AppsContainerView";
}

void AppsContainerView::OnBoundsChanged(const gfx::Rect& old_bounds) {
  const bool creating_initial_config = !app_list_config_;

  // The size and layout of apps grid items depend on the dimensions of the
  // display on which the apps container is shown. Given that the apps container
  // is shown in fullscreen app list view (and covers complete app list view
  // bounds), changes in the `AppsContainerView` bounds can be used as a proxy
  // to detect display size changes.
  UpdateAppListConfig(GetContentsBounds());
  DCHECK(app_list_config_);
  UpdateTopLevelGridDimensions();

  // Finish initialization of views that require app list config.
  if (creating_initial_config)
    UpdateForActiveAppListModel();
}

void AppsContainerView::OnGestureEvent(ui::GestureEvent* event) {
  // Ignore tap/long-press, allow those to pass to the ancestor view.
  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_LONG_PRESS) {
    return;
  }

  // Will forward events to |apps_grid_view_| if they occur in the same y-region
  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN &&
      event->location().y() <= apps_grid_view_->bounds().y()) {
    return;
  }

  // If a folder is currently opening or closing, we should ignore the event.
  // This is here until the animation for pagination while closing folders is
  // fixed: https://crbug.com/875133
  if (app_list_folder_view_->IsAnimationRunning()) {
    event->SetHandled();
    return;
  }

  // Temporary event for use by |apps_grid_view_|
  ui::GestureEvent grid_event(*event);
  ConvertEventToTarget(apps_grid_view_, &grid_event);
  apps_grid_view_->OnGestureEvent(&grid_event);

  // If the temporary event was handled, we don't want to handle it again.
  if (grid_event.handled())
    event->SetHandled();
}

void AppsContainerView::OnShown() {
  DVLOG(1) << __FUNCTION__;
  // Explicitly hide the virtual keyboard before showing the apps container
  // view. This prevents the virtual keyboard's "transient blur" feature from
  // kicking in - if a text input loses focus, and a text input gains it within
  // seconds, the virtual keyboard gets reshown. This is undesirable behavior
  // for the app list (where search box gets focus by default).
  if (keyboard::KeyboardUIController::HasInstance())
    keyboard::KeyboardUIController::Get()->HideKeyboardExplicitlyBySystem();

  GetViewAccessibility().OverrideIsLeaf(false);
}

void AppsContainerView::OnWillBeHidden() {
  DVLOG(1) << __FUNCTION__;
  if (show_state_ == SHOW_APPS || show_state_ == SHOW_ITEM_REPARENT)
    apps_grid_view_->EndDrag(true);
  else if (show_state_ == SHOW_ACTIVE_FOLDER)
    app_list_folder_view_->CloseFolderPage();
}

void AppsContainerView::OnHidden() {
  // Apps container view is shown faded behind the search results UI - hide its
  // contents from the screen reader as the apps grid is not normally
  // actionable in this state.
  GetViewAccessibility().OverrideIsLeaf(true);
}

void AppsContainerView::OnAnimationStarted(AppListState from_state,
                                           AppListState to_state) {
  gfx::Rect contents_bounds = GetDefaultContentsBounds();

  const gfx::Rect from_rect =
      GetPageBoundsForState(from_state, contents_bounds, gfx::Rect());
  const gfx::Rect to_rect =
      GetPageBoundsForState(to_state, contents_bounds, gfx::Rect());
  if (from_rect != to_rect) {
    DCHECK_EQ(from_rect.size(), to_rect.size());
    DCHECK_EQ(from_rect.x(), to_rect.x());

    SetBoundsRect(to_rect);

    gfx::Transform initial_transform;
    initial_transform.Translate(0, from_rect.y() - to_rect.y());
    layer()->SetTransform(initial_transform);

    auto settings = contents_view_->CreateTransitionAnimationSettings(layer());
    layer()->SetTransform(gfx::Transform());
  }

  // Set the page opacity.
  auto settings = contents_view_->CreateTransitionAnimationSettings(layer());
  UpdateContainerOpacityForState(to_state);
}

void AppsContainerView::UpdatePageOpacityForState(AppListState state,
                                                  float search_box_opacity,
                                                  bool restore_opacity) {
  UpdateContainerOpacityForState(state);

  const float progress =
      contents_view_->app_list_view()->GetAppListTransitionProgress(
          AppListView::kProgressFlagNone);
  UpdateContentsOpacity(progress, restore_opacity);
}

void AppsContainerView::UpdatePageBoundsForState(
    AppListState state,
    const gfx::Rect& contents_bounds,
    const gfx::Rect& search_box_bounds) {
  AppListPage::UpdatePageBoundsForState(state, contents_bounds,
                                        search_box_bounds);

  const float progress =
      contents_view_->app_list_view()->GetAppListTransitionProgress(
          AppListView::kProgressFlagNone);
  UpdateContentsYPosition(progress);
}

gfx::Rect AppsContainerView::GetPageBoundsForState(
    AppListState state,
    const gfx::Rect& contents_bounds,
    const gfx::Rect& search_box_bounds) const {
  if (state == AppListState::kStateApps)
    return contents_bounds;

  gfx::Rect bounds = contents_bounds;
  bounds.Offset(0, kNonAppsStateVerticalOffset);
  return bounds;
}

int AppsContainerView::GetMinHorizontalMarginForAppsGrid() const {
  return kPageSwitcherEndMargin + kGridToPageSwitcherMargin +
         page_switcher_->GetPreferredSize().width();
}

int AppsContainerView::GetMinTopMarginForAppsGrid(
    const gfx::Size& search_box_size) const {
  const int suggestion_chip_container_size =
      features::IsProductivityLauncherEnabled()
          ? 0
          : kSuggestionChipContainerHeight;
  // NOTE: Use the fadeout zone height as min top margin to match the apps grid
  // view's bottom margin.
  return search_box_size.height() + kGridFadeoutZoneHeight +
         kSuggestionChipContainerTopMargin + suggestion_chip_container_size;
}

int AppsContainerView::GetIdealHorizontalMargin() const {
  const int available_width = GetContentsBounds().width();
  if (available_width >=
      kAppsGridMarginRatio * GetMinHorizontalMarginForAppsGrid()) {
    return available_width / kAppsGridMarginRatio;
  }
  return available_width / kAppsGridMarginRatioForSmallWidth;
}

int AppsContainerView::GetIdealVerticalMargin() const {
  return GetContentsBounds().height() / kAppsGridMarginRatio;
}

const gfx::Insets& AppsContainerView::CalculateMarginsForAvailableBounds(
    const gfx::Rect& available_bounds,
    const gfx::Size& search_box_size) {
  if (cached_container_margins_.bounds_size == available_bounds.size() &&
      cached_container_margins_.search_box_size == search_box_size) {
    return cached_container_margins_.margins;
  }

  // For productivity launcher, the `grid_layout`'s rows will be ignored because
  // the vertical margin will be constant.
  const GridLayout grid_layout = CalculateGridLayout();
  const gfx::Size min_grid_size = apps_grid_view()->GetMinimumTileGridSize(
      grid_layout.columns, grid_layout.rows);
  const gfx::Size max_grid_size = apps_grid_view()->GetMaximumTileGridSize(
      grid_layout.columns, grid_layout.rows);

  int available_height = available_bounds.height();
  // Add search box, and suggestion chips container height (with its margins to
  // search box and apps grid) to non apps grid size.
  // NOTE: Not removing bottom apps grid inset because they are included into
  // the total margin values.
  available_height -= GetMinTopMarginForAppsGrid(search_box_size);

  // Calculates margin value to ensure the apps grid size is within required
  // bounds.
  // |ideal_margin|: The value the margin would have with no restrictions on
  //                 grid size.
  // |available_size|: The available size for apps grid in the dimension where
  //                   margin is applied.
  // |min_size|: The min allowed size for apps grid in the dimension where
  //             margin is applied.
  // |max_size|: The max allowed size for apps grid in the dimension where
  //             margin is applied.
  const auto calculate_margin = [](int ideal_margin, int available_size,
                                   int min_size, int max_size) -> int {
    const int ideal_size = available_size - 2 * ideal_margin;
    if (ideal_size < min_size)
      return ideal_margin - (min_size - ideal_size + 1) / 2;
    if (ideal_size > max_size)
      return ideal_margin + (ideal_size - max_size) / 2;
    return ideal_margin;
  };

  int vertical_margin = 0;
  if (features::IsProductivityLauncherEnabled()) {
    // Productivity launcher does not have a preset number of rows per page.
    // Instead of adjusting the margins to fit a set number of rows, the grid
    // will change the number of rows to fit within the provided space.
    vertical_margin = kGridFadeoutZoneHeight;
  } else {
    vertical_margin =
        calculate_margin(GetIdealVerticalMargin(), available_height,
                         min_grid_size.height(), max_grid_size.height());
  }

  const int horizontal_margin =
      calculate_margin(GetIdealHorizontalMargin(), available_bounds.width(),
                       min_grid_size.width(), max_grid_size.width());

  const int min_horizontal_margin = GetMinHorizontalMarginForAppsGrid();

  cached_container_margins_.margins =
      gfx::Insets(std::max(vertical_margin, kGridFadeoutZoneHeight),
                  std::max(horizontal_margin, min_horizontal_margin),
                  std::max(vertical_margin, kGridFadeoutZoneHeight),
                  std::max(horizontal_margin, min_horizontal_margin));
  cached_container_margins_.bounds_size = available_bounds.size();
  cached_container_margins_.search_box_size = search_box_size;

  return cached_container_margins_.margins;
}

void AppsContainerView::UpdateRecentApps() {
  if (!GetRecentApps() || !app_list_config_)
    return;

  AppListModelProvider* const model_provider = AppListModelProvider::Get();
  GetRecentApps()->ShowResults(model_provider->search_model(),
                               model_provider->model());
}

void AppsContainerView::UpdateSuggestionChips() {
  if (!suggestion_chip_container_view_)
    return;

  suggestion_chip_container_view_->SetResults(
      AppListModelProvider::Get()->search_model()->results());
}

base::ScopedClosureRunner AppsContainerView::DisableSuggestionChipsBlur() {
  if (!suggestion_chip_container_view_)
    return base::ScopedClosureRunner(base::DoNothing());

  ++suggestion_chips_blur_disabler_count_;

  if (suggestion_chips_blur_disabler_count_ == 1)
    suggestion_chip_container_view_->SetBlurDisabled(true);

  return base::ScopedClosureRunner(
      base::BindOnce(&AppsContainerView::OnSuggestionChipsBlurDisablerReleased,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AppsContainerView::SetShowState(ShowState show_state,
                                     bool show_apps_with_animation) {
  if (show_state_ == show_state)
    return;

  show_state_ = show_state;

  // Layout before showing animation because the animation's target bounds are
  // calculated based on the layout.
  Layout();

  switch (show_state_) {
    case SHOW_APPS:
      page_switcher_->SetCanProcessEventsWithinSubtree(true);
      folder_background_view_->SetVisible(false);
      apps_grid_view_->ResetForShowApps();
      app_list_folder_view_->ResetItemsGridForClose();
      if (show_apps_with_animation) {
        app_list_folder_view_->ScheduleShowHideAnimation(false, false);
      } else {
        app_list_folder_view_->HideViewImmediately();
      }
      break;
    case SHOW_ACTIVE_FOLDER:
      page_switcher_->SetCanProcessEventsWithinSubtree(false);
      folder_background_view_->SetVisible(true);
      app_list_folder_view_->ScheduleShowHideAnimation(true, false);
      break;
    case SHOW_ITEM_REPARENT:
      page_switcher_->SetCanProcessEventsWithinSubtree(true);
      folder_background_view_->SetVisible(false);
      app_list_folder_view_->ScheduleShowHideAnimation(false, true);
      break;
    default:
      NOTREACHED();
  }
}

void AppsContainerView::UpdateContainerOpacityForState(AppListState state) {
  const float target_opacity =
      state == AppListState::kStateApps ? 1.0f : kNonAppsStateOpacity;
  if (layer()->GetTargetOpacity() != target_opacity)
    layer()->SetOpacity(target_opacity);
}

void AppsContainerView::UpdateContentsOpacity(float progress,
                                              bool restore_opacity) {
  apps_grid_view_->UpdateOpacity(restore_opacity, kAppsOpacityChangeStart,
                                 kAppsOpacityChangeEnd);

  // Updates the opacity of page switcher buttons. The same rule as all apps in
  // AppsGridView.
  AppListView* app_list_view = contents_view_->app_list_view();
  int screen_bottom = app_list_view->GetScreenBottom();
  gfx::Rect switcher_bounds = page_switcher_->GetBoundsInScreen();
  float centerline_above_work_area =
      std::max<float>(screen_bottom - switcher_bounds.CenterPoint().y(), 0.f);
  float opacity =
      std::min(std::max((centerline_above_work_area - kAppsOpacityChangeStart) /
                            (kAppsOpacityChangeEnd - kAppsOpacityChangeStart),
                        0.f),
               1.0f);
  page_switcher_->layer()->SetOpacity(restore_opacity ? 1.0f : opacity);

  if (suggestion_chip_container_view_) {
    // Changes the opacity of suggestion chips between 0 and 1 when app list
    // transition progress changes between |kSuggestionChipOpacityStartProgress|
    // and |kSuggestionChipOpacityEndProgress|.
    float chips_opacity =
        base::clamp((progress - kSuggestionChipOpacityStartProgress) /
                        (kSuggestionChipOpacityEndProgress -
                         kSuggestionChipOpacityStartProgress),
                    0.0f, 1.0f);
    suggestion_chip_container_view_->layer()->SetOpacity(
        restore_opacity ? 1.0 : chips_opacity);
  }
}

void AppsContainerView::UpdateContentsYPosition(float progress) {
  const int current_suggestion_chip_y = GetExpectedSuggestionChipY(progress);
  if (suggestion_chip_container_view_)
    suggestion_chip_container_view_->SetY(current_suggestion_chip_y);
  scrollable_container_->SetY(current_suggestion_chip_y +
                              chip_grid_y_distance_);
  page_switcher_->SetY(current_suggestion_chip_y + chip_grid_y_distance_);

  // If app list is in drag, reset transforms that might started animating in
  // AnimateYPosition().
  if (contents_view_->app_list_view()->is_in_drag()) {
    if (suggestion_chip_container_view_)
      suggestion_chip_container_view_->layer()->SetTransform(gfx::Transform());
    scrollable_container_->layer()->SetTransform(gfx::Transform());
    page_switcher_->layer()->SetTransform(gfx::Transform());
  }
}

void AppsContainerView::DisableFocusForShowingActiveFolder(bool disabled) {
  if (suggestion_chip_container_view_) {
    suggestion_chip_container_view_->DisableFocusForShowingActiveFolder(
        disabled);
  }
  apps_grid_view_->DisableFocusForShowingActiveFolder(disabled);

  // Ignore the page switcher in accessibility tree so that buttons inside it
  // will not be accessed by ChromeVox.
  SetViewIgnoredForAccessibility(page_switcher_, disabled);
}

int AppsContainerView::GetExpectedSuggestionChipY(float progress) {
  const gfx::Rect search_box_bounds =
      contents_view_->GetSearchBoxExpectedBoundsForProgress(
          AppListState::kStateApps, progress);

  if (!suggestion_chip_container_view_)
    return search_box_bounds.bottom();

  return search_box_bounds.bottom() + kSuggestionChipContainerTopMargin;
}

AppsContainerView::GridLayout AppsContainerView::CalculateGridLayout() const {
  DCHECK(GetWidget());

  // Adapt columns and rows based on the display/root window size.
  const gfx::Size size =
      display::Screen::GetScreen()
          ->GetDisplayNearestView(GetWidget()->GetNativeView())
          .work_area()
          .size();
  const bool is_portrait_mode = size.height() > size.width();
  const int available_height =
      CalculateAvailableBoundsForAppsGrid(GetContentsBounds()).height();

  int preferred_columns = 0;
  int preferred_rows = 0;

  if (is_portrait_mode) {
    preferred_rows = features::IsProductivityLauncherEnabled()
                         ? kPreferredGridRowsInPortraitProductivityLauncher
                         : kPreferredGridColumns;
    preferred_columns =
        features::IsProductivityLauncherEnabled()
            ? kPreferredGridColumnsInPortraitProductivityLauncher
            : kPreferredGridRows;
  } else {
    preferred_rows = kPreferredGridRows;
    preferred_columns = kPreferredGridColumns;
  }

  GridLayout result;
  result.columns = preferred_columns;
  result.rows =
      apps_grid_view_->CalculateMaxRows(available_height, preferred_rows);
  result.first_page_rows = apps_grid_view_->CalculateFirstPageMaxRows(
      available_height, preferred_rows);
  return result;
}

void AppsContainerView::UpdateForActiveAppListModel() {
  AppListModel* const model = AppListModelProvider::Get()->model();
  apps_grid_view_->SetModel(model);
  apps_grid_view_->SetItemList(model->top_level_item_list());
  UpdateRecentApps();
  UpdateSuggestionChips();

  // If model changes, close the folder view if it's open, as the associated
  // item list is about to go away.
  SetShowState(SHOW_APPS, false);
}

void AppsContainerView::OnSuggestionChipsBlurDisablerReleased() {
  DCHECK_GT(suggestion_chips_blur_disabler_count_, 0u);
  --suggestion_chips_blur_disabler_count_;

  if (suggestion_chips_blur_disabler_count_ == 0)
    suggestion_chip_container_view_->SetBlurDisabled(false);
}

}  // namespace ash
