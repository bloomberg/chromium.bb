// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/start_page_view.h"

#include <algorithm>
#include <string>

#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/search_result.h"
#include "ui/app_list/views/all_apps_tile_item_view.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/custom_launcher_page_view.h"
#include "ui/app_list/views/indicator_chip_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/search_result_container_view.h"
#include "ui/app_list/views/search_result_tile_item_view.h"
#include "ui/app_list/views/suggestions_container_view.h"
#include "ui/app_list/views/tile_item_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace app_list {

namespace {

// Layout constants.
constexpr int kSearchBoxTopPadding = 24;
constexpr int kInstantContainerSpacing = 24;
constexpr int kSearchBoxAndTilesSpacing = 35;
constexpr int kSearchBoxAndIndicatorSpacing = 21;
constexpr int kStartPageSearchBoxWidth = 480;
constexpr int kStartPageSearchBoxWidthFullscreen = 544;

// WebView constants.
constexpr int kWebViewWidth = 700;
constexpr int kWebViewHeight = 224;

constexpr int kLauncherPageBackgroundWidth = 400;

}  // namespace

class CustomLauncherPageBackgroundView : public views::View {
 public:
  explicit CustomLauncherPageBackgroundView(
      const std::string& custom_launcher_page_name)
      : selected_(false),
        custom_launcher_page_name_(custom_launcher_page_name) {
    SetBackground(views::CreateSolidBackground(kSelectedColor));
  }
  ~CustomLauncherPageBackgroundView() override {}

  void SetSelected(bool selected) {
    selected_ = selected;
    SetVisible(selected);
    if (selected)
      NotifyAccessibilityEvent(ui::AX_EVENT_SELECTION, true);
  }

  bool selected() { return selected_; }

  // Overridden from views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ui::AX_ROLE_BUTTON;
    node_data->SetName(custom_launcher_page_name_);
  }

 private:
  bool selected_;
  std::string custom_launcher_page_name_;

  DISALLOW_COPY_AND_ASSIGN(CustomLauncherPageBackgroundView);
};

////////////////////////////////////////////////////////////////////////////////
// StartPageView implementation:
StartPageView::StartPageView(AppListMainView* app_list_main_view,
                             AppListViewDelegate* view_delegate,
                             AppListView* app_list_view)
    : app_list_main_view_(app_list_main_view),
      view_delegate_(view_delegate),
      search_box_spacer_view_(new View()),
      instant_container_(new views::View),
      custom_launcher_page_background_(new CustomLauncherPageBackgroundView(
          view_delegate_->GetModel()->custom_launcher_page_name())),
      suggestions_container_(new SuggestionsContainerView(
          app_list_main_view->contents_view(),
          new AllAppsTileItemView(app_list_main_view_->contents_view(),
                                  app_list_view))),
      is_fullscreen_app_list_enabled_(features::IsFullscreenAppListEnabled()) {
  search_box_spacer_view_->SetPreferredSize(gfx::Size(
      is_fullscreen_app_list_enabled_ ? kStartPageSearchBoxWidthFullscreen
                                      : kStartPageSearchBoxWidth,
      app_list_main_view->search_box_view()->GetPreferredSize().height()));

  // The view containing the start page WebContents and SearchBoxSpacerView.
  InitInstantContainer();
  AddChildView(instant_container_);

  if (is_fullscreen_app_list_enabled_) {
    indicator_ = new IndicatorChipView(
        l10n_util::GetStringUTF16(IDS_SUGGESTED_APPS_INDICATOR));
    AddChildView(indicator_);
  }

  // The view containing the start page tiles.
  AddChildView(suggestions_container_);
  AddChildView(custom_launcher_page_background_);

  suggestions_container_->SetResults(view_delegate_->GetModel()->results());
}

StartPageView::~StartPageView() = default;

void StartPageView::InitInstantContainer() {
  views::BoxLayout* instant_layout_manager = new views::BoxLayout(
      views::BoxLayout::kVertical, gfx::Insets(), kInstantContainerSpacing);
  if (is_fullscreen_app_list_enabled_) {
    instant_layout_manager->set_inside_border_insets(
        gfx::Insets(kSearchBoxTopPadding, 0, kSearchBoxAndIndicatorSpacing, 0));
  } else {
    instant_layout_manager->set_inside_border_insets(
        gfx::Insets(0, 0, kSearchBoxAndTilesSpacing, 0));
  }
  instant_layout_manager->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_END);
  instant_layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  instant_container_->SetLayoutManager(instant_layout_manager);

  // Create the view for the Google Doodle if the fullscreen launcher is not
  // enabled.
  if (!is_fullscreen_app_list_enabled_) {
    views::View* web_view = view_delegate_->CreateStartPageWebView(
        gfx::Size(kWebViewWidth, kWebViewHeight));

    if (web_view) {
      web_view->SetFocusBehavior(FocusBehavior::NEVER);
      instant_container_->AddChildView(web_view);
    }
  }

  instant_container_->AddChildView(search_box_spacer_view_);
}

void StartPageView::MaybeOpenCustomLauncherPage() {
  // Switch to the custom page.
  ContentsView* contents_view = app_list_main_view_->contents_view();
  if (!app_list_main_view_->ShouldShowCustomLauncherPage())
    return;

  UMA_HISTOGRAM_ENUMERATION(kPageOpenedHistogram,
                            AppListModel::STATE_CUSTOM_LAUNCHER_PAGE,
                            AppListModel::STATE_LAST);

  contents_view->SetActiveState(AppListModel::STATE_CUSTOM_LAUNCHER_PAGE);
}

void StartPageView::Reset() {
  suggestions_container_->Update();
}

void StartPageView::UpdateForTesting() {
  suggestions_container_->Update();
}

const std::vector<SearchResultTileItemView*>& StartPageView::tile_views()
    const {
  return suggestions_container_->tile_views();
}

TileItemView* StartPageView::all_apps_button() const {
  return suggestions_container_->all_apps_button();
}

void StartPageView::OnShown() {
  // When the start page is shown, show or hide the custom launcher page
  // based on whether it is enabled.
  CustomLauncherPageView* custom_page_view =
      app_list_main_view_->contents_view()->custom_page_view();
  if (custom_page_view) {
    custom_page_view->SetVisible(
        app_list_main_view_->ShouldShowCustomLauncherPage());
  }
  suggestions_container_->ClearSelectedIndex();
  suggestions_container_->Update();
  custom_launcher_page_background_->SetSelected(false);
}

gfx::Rect StartPageView::GetPageBoundsForState(
    AppListModel::State state) const {
  gfx::Rect onscreen_bounds(GetFullContentsBounds());
  if (state == AppListModel::STATE_START)
    return onscreen_bounds;

  return GetAboveContentsOffscreenBounds(onscreen_bounds.size());
}

gfx::Rect StartPageView::GetSearchBoxBounds() const {
  return search_box_spacer_view_->bounds() +
         GetPageBoundsForState(AppListModel::STATE_START).OffsetFromOrigin();
}

void StartPageView::Layout() {
  gfx::Rect bounds(GetContentsBounds());
  bounds.set_height(instant_container_->GetHeightForWidth(bounds.width()));
  instant_container_->SetBoundsRect(bounds);

  // For old launcher, tiles begin where the instant container ends; for
  // fullscreen app list launcher, tiles begin where the |indicator_| ends.
  bounds.set_y(bounds.bottom());
  if (indicator_) {
    gfx::Rect indicator_rect(bounds);
    indicator_rect.Offset(
        (indicator_rect.width() - indicator_->GetPreferredSize().width()) / 2,
        0);
    indicator_->SetBoundsRect(indicator_rect);
    bounds.Inset(0, indicator_->GetPreferredSize().height(), 0, 0);
  }
  bounds.set_height(suggestions_container_->GetHeightForWidth(bounds.width()));
  suggestions_container_->SetBoundsRect(bounds);

  CustomLauncherPageView* custom_launcher_page_view =
      app_list_main_view_->contents_view()->custom_page_view();
  if (!custom_launcher_page_view)
    return;

  bounds = app_list_main_view_->contents_view()
               ->custom_page_view()
               ->GetCollapsedLauncherPageBounds();
  bounds.Intersect(GetContentsBounds());
  bounds.ClampToCenteredSize(
      gfx::Size(kLauncherPageBackgroundWidth, bounds.height()));
  custom_launcher_page_background_->SetBoundsRect(bounds);
}

bool StartPageView::OnKeyPressed(const ui::KeyEvent& event) {
  const int forward_dir = base::i18n::IsRTL() ? -1 : 1;
  int selected_index = suggestions_container_->selected_index();

  if (custom_launcher_page_background_->selected()) {
    selected_index = suggestions_container_->num_results();
    switch (event.key_code()) {
      case ui::VKEY_RETURN:
        MaybeOpenCustomLauncherPage();
        return true;
      default:
        break;
    }
  } else if (selected_index >= 0 &&
             suggestions_container_->GetTileItemView(selected_index)
                 ->OnKeyPressed(event)) {
    return true;
  }

  int dir = 0;
  switch (event.key_code()) {
    case ui::VKEY_LEFT:
      dir = -forward_dir;
      break;
    case ui::VKEY_RIGHT:
      // Don't go to the custom launcher page from the All apps tile.
      if (selected_index != suggestions_container_->num_results() - 1)
        dir = forward_dir;
      break;
    case ui::VKEY_UP:
      // Up selects the first tile if the custom launcher is selected.
      if (custom_launcher_page_background_->selected()) {
        selected_index = -1;
        dir = 1;
      }
      break;
    case ui::VKEY_DOWN:
      // Down selects the first tile if nothing is selected.
      dir = 1;
      // If something is selected, select the custom launcher page.
      if (suggestions_container_->IsValidSelectionIndex(selected_index))
        selected_index = suggestions_container_->num_results() - 1;
      break;
    case ui::VKEY_TAB:
      dir = event.IsShiftDown() ? -1 : 1;
      break;
    default:
      break;
  }

  if (dir == 0)
    return false;

  if (selected_index == -1) {
    custom_launcher_page_background_->SetSelected(false);
    suggestions_container_->SetSelectedIndex(
        dir == -1 ? suggestions_container_->num_results() - 1 : 0);
    return true;
  }

  int selection_index = selected_index + dir;
  if (suggestions_container_->IsValidSelectionIndex(selection_index)) {
    custom_launcher_page_background_->SetSelected(false);
    suggestions_container_->SetSelectedIndex(selection_index);
    return true;
  }

  if (selection_index == suggestions_container_->num_results() &&
      app_list_main_view_->ShouldShowCustomLauncherPage()) {
    custom_launcher_page_background_->SetSelected(true);
    suggestions_container_->ClearSelectedIndex();
    return true;
  }

  if (event.key_code() == ui::VKEY_TAB && selection_index == -1)
    suggestions_container_
        ->ClearSelectedIndex();  // ContentsView will handle focus.

  return false;
}

bool StartPageView::OnMousePressed(const ui::MouseEvent& event) {
  ContentsView* contents_view = app_list_main_view_->contents_view();
  if (contents_view->custom_page_view() &&
      !contents_view->custom_page_view()
           ->GetCollapsedLauncherPageBounds()
           .Contains(event.location())) {
    return false;
  }

  MaybeOpenCustomLauncherPage();
  return true;
}

bool StartPageView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  // Negative y_offset is a downward scroll.
  if (event.y_offset() < 0) {
    MaybeOpenCustomLauncherPage();
    return true;
  }

  return false;
}

void StartPageView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN &&
      event->details().scroll_y_hint() < 0) {
    MaybeOpenCustomLauncherPage();
  }

  ContentsView* contents_view = app_list_main_view_->contents_view();
  if (event->type() == ui::ET_GESTURE_TAP &&
      contents_view->custom_page_view() &&
      contents_view->custom_page_view()
          ->GetCollapsedLauncherPageBounds()
          .Contains(event->location())) {
    MaybeOpenCustomLauncherPage();
  }
}

void StartPageView::OnScrollEvent(ui::ScrollEvent* event) {
  // Negative y_offset is a downward scroll (or upward, if Australian Scrolling
  // is enabled).
  if (event->type() == ui::ET_SCROLL && event->y_offset() < 0)
    MaybeOpenCustomLauncherPage();
}

TileItemView* StartPageView::GetTileItemView(size_t index) {
  return suggestions_container_->GetTileItemView(index);
}

}  // namespace app_list
