// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_page_view.h"

#include <stddef.h>

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/search_result_list_view.h"
#include "ui/app_list/views/search_result_tile_item_list_view.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/shadow_border.h"

namespace app_list {

namespace {

constexpr int kGroupSpacing = 6;
constexpr int kTopPadding = 8;
constexpr int kFullscreenHeight = 440;
constexpr int kFullscreenWidth = 640;

// The z-height of the search box and cards in this view.
constexpr int kSearchResultZHeight = 1;

// The horizontal padding of the separator.
constexpr int kSeparatorPadding = 12;
constexpr int kSeparatorThickness = 1;

// The height of the search box in this page.
constexpr int kSearchBoxHeight = 56;

constexpr SkColor kSeparatorColor = SkColorSetARGBMacro(0x1F, 0x00, 0x00, 0x00);

// A container view that ensures the card background and the shadow are painted
// in the correct order.
class SearchCardView : public views::View {
 public:
  explicit SearchCardView(views::View* content_view) {
    if (!features::IsFullscreenAppListEnabled()) {
      SetBorder(base::MakeUnique<views::ShadowBorder>(
          GetShadowForZHeight(kSearchResultZHeight)));
      content_view->SetBackground(
          views::CreateSolidBackground(kCardBackgroundColor));
    }
    SetLayoutManager(new views::FillLayout());
    AddChildView(content_view);
  }

  // views::View overrides:
  const char* GetClassName() const override { return "SearchCardView"; }

  ~SearchCardView() override {}
};

class ZeroWidthVerticalScrollBar : public views::OverlayScrollBar {
 public:
  ZeroWidthVerticalScrollBar() : OverlayScrollBar(false) {}

  // OverlayScrollBar overrides:
  int GetThickness() const override { return 0; }

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    if (!features::IsAppListFocusEnabled())
      return OverlayScrollBar::OnKeyPressed(event);

    // Arrow keys should be handled by FocusManager to move focus. When a search
    // result is focued, it will be set visible in scroll view.
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ZeroWidthVerticalScrollBar);
};

class SearchResultPageBackground : public views::Background {
 public:
  SearchResultPageBackground(SkColor color, int corner_radius)
      : color_(color), corner_radius_(corner_radius) {}
  ~SearchResultPageBackground() override {}

 private:
  // views::Background overrides:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    gfx::Rect bounds = view->GetContentsBounds();
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(color_);
    canvas->DrawRoundRect(bounds, corner_radius_, flags);

    if (bounds.height() <= kSearchBoxHeight)
      return;
    // Draw a separator between SearchBoxView and SearchResultPageView.
    bounds.set_y(kSearchBoxHeight);
    bounds.set_height(kSeparatorThickness);
    canvas->FillRect(bounds, kSeparatorColor);
  }

  const SkColor color_;
  const int corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultPageBackground);
};

}  // namespace

class SearchResultPageView::HorizontalSeparator : public views::View {
 public:
  explicit HorizontalSeparator(int preferred_width)
      : preferred_width_(preferred_width) {
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets(0, kSeparatorPadding, 0, kSeparatorPadding)));
  }

  ~HorizontalSeparator() override {}

  // views::View overrides:
  const char* GetClassName() const override { return "HorizontalSeparator"; }

  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(preferred_width_, kSeparatorThickness);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    gfx::Rect rect = GetContentsBounds();
    canvas->FillRect(rect, kSeparatorColor);
    View::OnPaint(canvas);
  }

 private:
  const int preferred_width_;

  DISALLOW_COPY_AND_ASSIGN(HorizontalSeparator);
};

SearchResultPageView::SearchResultPageView()
    : selected_index_(0),
      is_fullscreen_app_list_enabled_(features::IsFullscreenAppListEnabled()),
      is_app_list_focus_enabled_(features::IsAppListFocusEnabled()),
      contents_view_(new views::View) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  if (is_fullscreen_app_list_enabled_) {
    contents_view_->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, gfx::Insets(), 0));

    // Hides this view behind the search box by using the same color and
    // background border corner radius. All child views' background should be
    // set transparent so that the rounded corner is not overwritten.
    SetBackground(base::MakeUnique<SearchResultPageBackground>(
        kCardBackgroundColorFullscreen,
        kSearchBoxBorderCornerRadiusFullscreen));
  } else {
    gfx::ShadowValue shadow = GetShadowForZHeight(kSearchResultZHeight);
    std::unique_ptr<views::Border> border(new views::ShadowBorder(shadow));

    gfx::Insets insets =
        gfx::Insets(kTopPadding, kSearchBoxPadding, 0, kSearchBoxPadding);
    insets += -border->GetInsets();

    views::BoxLayout* layout = new views::BoxLayout(
        views::BoxLayout::kVertical, gfx::Insets(), kGroupSpacing);
    layout->set_inside_border_insets(insets);
    contents_view_->SetLayoutManager(layout);
  }

  views::ScrollView* const scroller = new views::ScrollView;
  if (is_fullscreen_app_list_enabled_) {
    // Leaves a placeholder area for the search box and the separator below it.
    scroller->SetBorder(views::CreateEmptyBorder(
        gfx::Insets(kSearchBoxHeight + kSeparatorThickness, 0, 0, 0)));
    scroller->set_draw_overflow_indicator(false);
  }
  scroller->SetContents(contents_view_);
  // Setting clip height is necessary to make ScrollView take into account its
  // contents' size. Using zeroes doesn't prevent it from scrolling and sizing
  // correctly.
  scroller->ClipHeightTo(0, 0);
  scroller->SetVerticalScrollBar(new ZeroWidthVerticalScrollBar);
  scroller->SetBackgroundColor(SK_ColorTRANSPARENT);
  AddChildView(scroller);

  SetLayoutManager(new views::FillLayout);
}

SearchResultPageView::~SearchResultPageView() = default;

void SearchResultPageView::SetSelection(bool select) {
  if (select)
    SetSelectedIndex(0, false);
  else
    ClearSelectedIndex();
}

void SearchResultPageView::AddSearchResultContainerView(
    AppListModel::SearchResults* results_model,
    SearchResultContainerView* result_container) {
  if (is_fullscreen_app_list_enabled_ && !result_container_views_.empty()) {
    HorizontalSeparator* separator = new HorizontalSeparator(bounds().width());
    contents_view_->AddChildView(separator);
    separators_.push_back(separator);
  }
  contents_view_->AddChildView(new SearchCardView(result_container));
  result_container_views_.push_back(result_container);
  result_container->SetResults(results_model);
  result_container->set_delegate(this);
}

bool SearchResultPageView::OnKeyPressed(const ui::KeyEvent& event) {
  if (is_app_list_focus_enabled_) {
    views::View* next_focusable_view = nullptr;
    if (event.key_code() == ui::VKEY_UP) {
      next_focusable_view = GetFocusManager()->GetNextFocusableView(
          GetFocusManager()->GetFocusedView(), GetWidget(), true, false);
    } else if (event.key_code() == ui::VKEY_DOWN) {
      next_focusable_view = GetFocusManager()->GetNextFocusableView(
          GetFocusManager()->GetFocusedView(), GetWidget(), false, false);
    }

    if (next_focusable_view && !Contains(next_focusable_view)) {
      // Hitting up key when focus is on first search result or hitting down
      // key when focus is on last search result should move focus onto search
      // box.
      AppListPage::contents_view()
          ->GetSearchBoxView()
          ->search_box()
          ->RequestFocus();
      return true;
    }

    // Return false to let FocusManager to handle default focus move by key
    // events.
    return false;
  }
  // TODO(weidongg/766807) Remove everything below when the flag is enabled by
  // default.
  if (HasSelection() &&
      result_container_views_.at(selected_index_)->OnKeyPressed(event)) {
    return true;
  }

  int dir = 0;
  bool directional_movement = false;
  const int forward_dir = base::i18n::IsRTL() ? -1 : 1;
  switch (event.key_code()) {
    case ui::VKEY_TAB:
      dir = event.IsShiftDown() ? -1 : 1;
      break;
    case ui::VKEY_UP:
      dir = -1;
      directional_movement = true;
      break;
    case ui::VKEY_DOWN:
      dir = 1;
      directional_movement = true;
      break;
    case ui::VKEY_LEFT:
      if (!is_fullscreen_app_list_enabled_)
        return false;
      dir = -forward_dir;
      break;
    case ui::VKEY_RIGHT:
      if (!is_fullscreen_app_list_enabled_)
        return false;
      dir = forward_dir;
      break;
    default:
      return false;
  }

  // Find the next result container with results.
  int new_selected = selected_index_;
  do {
    new_selected += dir;
  } while (IsValidSelectionIndex(new_selected) &&
           result_container_views_[new_selected]->num_results() == 0);

  if (IsValidSelectionIndex(new_selected)) {
    SetSelectedIndex(new_selected, directional_movement);
    return true;
  }

  if (!is_fullscreen_app_list_enabled_)
    return false;

  if (dir == -1) {
    // Shift+tab/up/left key could move focus back to search box.
    ClearSelectedIndex();
  }
  return false;
}

const char* SearchResultPageView::GetClassName() const {
  return "SearchResultPageView";
}

gfx::Size SearchResultPageView::CalculatePreferredSize() const {
  if (!is_fullscreen_app_list_enabled_)
    return GetDefaultContentsBounds().size();
  return gfx::Size(kFullscreenWidth, kFullscreenHeight);
}

void SearchResultPageView::ClearSelectedIndex() {
  if (HasSelection())
    result_container_views_[selected_index_]->ClearSelectedIndex();

  selected_index_ = -1;
}

void SearchResultPageView::SetSelectedIndex(int index,
                                            bool directional_movement) {
  bool from_bottom = index < selected_index_;

  // Reset the old selected view's selection.
  ClearSelectedIndex();

  selected_index_ = index;
  // Set the new selected view's selection to its first result.
  result_container_views_[selected_index_]->OnContainerSelected(
      from_bottom, directional_movement);
}

bool SearchResultPageView::IsValidSelectionIndex(int index) {
  return index >= 0 && index < static_cast<int>(result_container_views_.size());
}

void SearchResultPageView::ReorderSearchResultContainers() {
  // Sort the result container views by their score.
  std::sort(result_container_views_.begin(), result_container_views_.end(),
            [](const SearchResultContainerView* a,
               const SearchResultContainerView* b) -> bool {
              return a->container_score() > b->container_score();
            });

  int result_y_index = 0;
  for (size_t i = 0; i < result_container_views_.size(); ++i) {
    SearchResultContainerView* view = result_container_views_[i];

    if (is_fullscreen_app_list_enabled_ && i > 0) {
      HorizontalSeparator* separator = separators_[i - 1];
      // Hides the separator above the container that has no results.
      if (!view->container_score())
        separator->SetVisible(false);
      else
        separator->SetVisible(true);

      contents_view_->ReorderChildView(separator, i * 2 - 1);
      contents_view_->ReorderChildView(view->parent(), i * 2);

      result_y_index += kSeparatorThickness;
    } else {
      contents_view_->ReorderChildView(view->parent(), i);
    }

    view->NotifyFirstResultYIndex(result_y_index);

    result_y_index += view->GetYSize();
  }

  Layout();
}

void SearchResultPageView::OnSearchResultContainerResultsChanged() {
  DCHECK(!result_container_views_.empty());
  if (is_fullscreen_app_list_enabled_)
    DCHECK(result_container_views_.size() == separators_.size() + 1);

  // Only sort and layout the containers when they have all updated.
  for (SearchResultContainerView* view : result_container_views_) {
    if (view->UpdateScheduled()) {
      return;
    }
  }

  if (is_app_list_focus_enabled_) {
    if (result_container_views_.empty())
      return;
    // Set the first result (if it exists) selected when search results are
    // updated. Note that the focus is not set on the first result to prevent
    // frequent focus switch between search box and first result during typing
    // query.
    SearchResultContainerView* old_first_container_view =
        result_container_views_[0];
    ReorderSearchResultContainers();
    old_first_container_view->SetFirstResultSelected(false);
    first_result_view_ =
        result_container_views_[0]->SetFirstResultSelected(true);
    return;
  }

  SearchResultContainerView* old_selection =
      HasSelection() ? result_container_views_[selected_index_] : nullptr;

  // Truncate the currently selected container's selection if necessary. If
  // there are no results, the selection will be cleared below.
  if (old_selection && old_selection->num_results() > 0 &&
      old_selection->selected_index() >= old_selection->num_results()) {
    old_selection->SetSelectedIndex(old_selection->num_results() - 1);
  }

  ReorderSearchResultContainers();

  SearchResultContainerView* new_selection = nullptr;
  if (HasSelection() &&
      result_container_views_[selected_index_]->num_results() > 0) {
    new_selection = result_container_views_[selected_index_];
  }

  // If there was no previous selection or the new view at the selection index
  // is different from the old one, update the selected view.
  if (!HasSelection() || old_selection != new_selection) {
    if (old_selection)
      old_selection->ClearSelectedIndex();

    int new_selection_index = new_selection ? selected_index_ : 0;
    // Clear the current selection so that the selection always comes in from
    // the top.
    ClearSelectedIndex();
    SetSelectedIndex(new_selection_index, false);
  }
}

gfx::Rect SearchResultPageView::GetPageBoundsForState(
    AppListModel::State state) const {
  if (!is_fullscreen_app_list_enabled_) {
    return (state == AppListModel::STATE_SEARCH_RESULTS
                ? GetDefaultContentsBounds()
                : GetAboveContentsOffscreenBounds(
                      GetDefaultContentsBounds().size()));
  }

  if (state != AppListModel::STATE_SEARCH_RESULTS) {
    // Hides this view behind the search box by using the same bounds.
    return AppListPage::contents_view()->GetSearchBoxBoundsForState(state);
  }

  gfx::Rect onscreen_bounds(AppListPage::GetSearchBoxBounds());
  onscreen_bounds.Offset((onscreen_bounds.width() - kFullscreenWidth) / 2, 0);
  onscreen_bounds.set_size(GetPreferredSize());
  return onscreen_bounds;
}

void SearchResultPageView::OnAnimationUpdated(double progress,
                                              AppListModel::State from_state,
                                              AppListModel::State to_state) {
  if (from_state != AppListModel::STATE_SEARCH_RESULTS &&
      to_state != AppListModel::STATE_SEARCH_RESULTS) {
    return;
  }

  if (is_fullscreen_app_list_enabled_) {
    const SearchBoxView* search_box =
        AppListPage::contents_view()->GetSearchBoxView();
    const SkColor color = gfx::Tween::ColorValueBetween(
        progress, search_box->GetBackgroundColorForState(from_state),
        search_box->GetBackgroundColorForState(to_state));

    // Grows this view in the same pace as the search box to make them look
    // like a single view.
    SetBackground(base::MakeUnique<SearchResultPageBackground>(
        color,
        gfx::Tween::LinearIntValueBetween(
            progress,
            search_box->GetSearchBoxBorderCornerRadiusForState(from_state),
            search_box->GetSearchBoxBorderCornerRadiusForState(to_state))));
  }

  gfx::Rect onscreen_bounds(
      GetPageBoundsForState(AppListModel::STATE_SEARCH_RESULTS));
  onscreen_bounds -= bounds().OffsetFromOrigin();
  gfx::Path path;
  path.addRect(gfx::RectToSkRect(onscreen_bounds));
  set_clip_path(path);
}

int SearchResultPageView::GetSearchBoxZHeight() const {
  return kSearchResultZHeight;
}

void SearchResultPageView::OnHidden() {
  ClearSelectedIndex();
}

gfx::Rect SearchResultPageView::GetSearchBoxBounds() const {
  gfx::Rect rect(AppListPage::GetSearchBoxBounds());
  if (is_fullscreen_app_list_enabled_) {
    rect.Offset((rect.width() - kFullscreenWidth) / 2, 0);
    rect.set_size(gfx::Size(kFullscreenWidth, kSearchBoxHeight));
  }
  return rect;
}

views::View* SearchResultPageView::GetSelectedView() const {
  if (!HasSelection())
    return nullptr;
  SearchResultContainerView* container =
      result_container_views_[selected_index_];
  return container->GetSelectedView();
}

}  // namespace app_list
